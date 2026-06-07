#include "overlay.hpp"
#include <cairo.h>
#include <dlfcn.h>
#include <gtk/gtk.h>
#include <gtk4-layer-shell/gtk4-layer-shell.h>
#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>

static void preload_layer_shell() {
    const char* paths[] = {"/usr/lib/libgtk4-layer-shell.so",
                           "/usr/lib64/libgtk4-layer-shell.so"};
    for (const char* p : paths) {
        if (void* h = dlopen(p, RTLD_NOW | RTLD_GLOBAL)) {
            dlclose(h);
            return;
        }
    }
}

static void gdk_click_through(GtkWidget* window) {
    GtkNative* native = gtk_widget_get_native(window);
    if (!native) return;
    GdkSurface* surface = gtk_native_get_surface(native);
    if (!surface) return;
    cairo_region_t* region = cairo_region_create();
    gdk_surface_set_input_region(surface, region);
    cairo_region_destroy(region);
}

static void apply_transparency_css(GtkWidget* win, GtkWidget* draw) {
    GtkCssProvider* provider = gtk_css_provider_new();
    const char* css = "window.sober-esp-overlay { background-color: transparent; }";
    gtk_css_provider_load_from_string(provider, css);
    GdkDisplay* display = gtk_widget_get_display(win);
    if (!display) return;
    gtk_style_context_add_provider_for_display(
        display, GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    gtk_widget_add_css_class(win, "sober-esp-overlay");
    gtk_widget_add_css_class(draw, "sober-esp-overlay");
    g_object_unref(provider);
}

static void clear_draw(cairo_t* cr, int width, int height) {
    if (width <= 0 || height <= 0) return;
    cairo_save(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_restore(cr);
}

EspOverlay::EspOverlay() {
    if (std::getenv("WAYLAND_DISPLAY")) preload_layer_shell();
}

EspOverlay::~EspOverlay() { stop(); }

std::pair<int, int> EspOverlay::display_size() const {
    return {sw_, sh_};
}

void EspOverlay::set_box_factory(BoxFactory factory) {
    std::lock_guard lock(factory_lock_);
    box_factory_ = std::move(factory);
}

void EspOverlay::clear_box_factory() {
    std::lock_guard lock(factory_lock_);
    box_factory_ = nullptr;
}

std::vector<EspBox> EspOverlay::boxes_for_draw() const {
    if (!running_.load() || !visible_.load()) return {};
    BoxFactory factory;
    {
        std::lock_guard lock(factory_lock_);
        factory = box_factory_;
    }
    if (!factory) return {};
    try {
        return factory();
    } catch (...) {
        return {};
    }
}

std::pair<int, int> EspOverlay::screen_size() const {
    return {sw_, sh_};
}

void EspOverlay::mark_ready(const std::string& msg) {
    ready_msg_ = msg;
    ready_ = true;
}

void EspOverlay::mark_ready_once(const std::string& msg) {
    bool expected = false;
    if (ready_.compare_exchange_strong(expected, true)) mark_ready(msg);
}

std::string EspOverlay::show(bool wait_ready, double timeout_sec) {
    visible_ = true;
    if (!running_) {
        ready_ = false;
        ready_msg_.clear();
        if (gui_thread_.joinable()) gui_thread_.join();
        gui_thread_ = std::thread(&EspOverlay::gui_thread_main, this);
        if (wait_ready) {
            auto deadline = std::chrono::steady_clock::now() +
                            std::chrono::duration<double>(timeout_sec);
            while (!ready_ && std::chrono::steady_clock::now() < deadline)
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    return ready_msg_;
}

void EspOverlay::hide() { visible_ = false; }

void EspOverlay::stop() {
    visible_ = false;
    running_ = false;
    gtk_ready_ = false;
    clear_box_factory();
    if (gui_thread_.joinable()) gui_thread_.join();
    if (gtk_ctx_) {
        g_main_context_unref(gtk_ctx_);
        gtk_ctx_ = nullptr;
    }
    layer_ = {};
}

void EspOverlay::gui_thread_main() {
    running_ = true;
    if (!try_gtk_layershell()) {
        std::cerr << "ESP: gtk4-layer-shell overlay failed\n";
        if (!ready_) mark_ready("");
    }
    running_ = false;
    gtk_ready_ = false;
}

static void draw_crosshair(cairo_t* cr, int width, int height,
                           const EspOverlay::CrosshairSettings& ch) {
    if (!ch.enabled.load(std::memory_order_relaxed)) return;
    float cx = width * 0.5f;
    float cy = height * 0.5f;
    float size = ch.size.load(std::memory_order_relaxed);
    float gap = ch.gap.load(std::memory_order_relaxed);
    cairo_set_source_rgb(cr, ch.r.load(std::memory_order_relaxed),
                         ch.g.load(std::memory_order_relaxed),
                         ch.b.load(std::memory_order_relaxed));
    cairo_set_line_width(cr, ch.thickness.load(std::memory_order_relaxed));
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_move_to(cr, cx + gap, cy);
    cairo_line_to(cr, cx + gap + size, cy);
    cairo_move_to(cr, cx - gap, cy);
    cairo_line_to(cr, cx - gap - size, cy);
    cairo_move_to(cr, cx, cy + gap);
    cairo_line_to(cr, cx, cy + gap + size);
    cairo_move_to(cr, cx, cy - gap);
    cairo_line_to(cr, cx, cy - gap - size);
    cairo_stroke(cr);
}

static void draw_fn(GtkDrawingArea* /*area*/, cairo_t* cr, int width, int height, gpointer user) {
    auto* ctx = static_cast<GtkLayerCtx*>(user);
    if (!ctx || !ctx->self) return;
    clear_draw(cr, width, height);
    if (!ctx->self->is_visible()) return;
    auto boxes = ctx->self->boxes_for_draw();
    cairo_set_line_width(cr, 2);
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 11);
    for (const auto& b : boxes) {
        if (b.fill && b.a > 0.f) {
            cairo_set_source_rgba(cr, b.r, b.g, b.b, b.a);
            cairo_rectangle(cr, b.x1, b.y1, b.x2 - b.x1, b.y2 - b.y1);
            cairo_fill(cr);
        }
        cairo_set_source_rgb(cr, b.r, b.g, b.b);
        cairo_rectangle(cr, b.x1, b.y1, b.x2 - b.x1, b.y2 - b.y1);
        cairo_stroke(cr);
        if (b.name.empty()) continue;
        cairo_text_extents_t te;
        cairo_text_extents(cr, b.name.c_str(), &te);
        double tx = (b.x1 + b.x2) * 0.5 + te.x_bearing - te.width * 0.5;
        double ty = b.y1 - 6;
        if (ty - te.height < 0) ty = b.y2 + te.height + 6;
        cairo_set_source_rgb(cr, 0, 0, 0);
        cairo_move_to(cr, tx - 1, ty - 1);
        cairo_show_text(cr, b.name.c_str());
        cairo_move_to(cr, tx + 1, ty - 1);
        cairo_show_text(cr, b.name.c_str());
        cairo_move_to(cr, tx - 1, ty + 1);
        cairo_show_text(cr, b.name.c_str());
        cairo_move_to(cr, tx + 1, ty + 1);
        cairo_show_text(cr, b.name.c_str());
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_move_to(cr, tx, ty);
        cairo_show_text(cr, b.name.c_str());
    }
    // Draw FOV circles
    auto draw_fov = [&](const EspOverlay::FovCircle& circle) {
        if (!circle.active.load(std::memory_order_relaxed)) return;
        float radius = circle.radius.load(std::memory_order_relaxed);
        if (radius <= 0.f) return;
        float cx = circle.cx.load(std::memory_order_relaxed);
        float cy = circle.cy.load(std::memory_order_relaxed);
        float fr = circle.r.load(std::memory_order_relaxed);
        float fg = circle.g.load(std::memory_order_relaxed);
        float fb = circle.b.load(std::memory_order_relaxed);
        float fa = circle.a.load(std::memory_order_relaxed);
        cairo_new_path(cr);
        cairo_move_to(cr, cx + radius, cy);
        cairo_set_line_width(cr, 1);
        cairo_set_source_rgba(cr, fr, fg, fb, 1.f);
        cairo_arc(cr, cx, cy, radius, 0, 2 * M_PI);
        cairo_stroke(cr);
        cairo_new_path(cr);
        cairo_move_to(cr, cx + radius, cy);
        cairo_set_source_rgba(cr, fr, fg, fb, fa);
        cairo_arc(cr, cx, cy, radius, 0, 2 * M_PI);
        cairo_fill(cr);
    };
    draw_fov(ctx->self->fov_);
    draw_crosshair(cr, width, height, ctx->self->crosshair_);
}

static gboolean tick(gpointer user) {
    auto* ctx = static_cast<GtkLayerCtx*>(user);
    if (!ctx || !ctx->self) return G_SOURCE_REMOVE;
    if (!ctx->self->is_running()) {
        g_main_loop_quit(static_cast<GMainLoop*>(g_object_get_data(G_OBJECT(ctx->win), "loop")));
        return G_SOURCE_REMOVE;
    }
    if (ctx->self->draw_due()) {
        gdk_click_through(ctx->win);
        gtk_widget_queue_draw(ctx->draw);
    }
    return G_SOURCE_CONTINUE;
}

static gboolean on_ready_idle(gpointer user) {
    auto* ctx = static_cast<GtkLayerCtx*>(user);
    if (!ctx || !ctx->self) return G_SOURCE_REMOVE;
    gdk_click_through(ctx->win);
    auto [sw, sh] = ctx->self->screen_size();
    ctx->self->mark_ready_once("ESP overlay ready (" + std::to_string(sw) + "x" +
                               std::to_string(sh) + ")");
    return G_SOURCE_REMOVE;
}

bool EspOverlay::try_gtk_layershell() {
    GtkWidget* win = gtk_window_new();
    gtk_window_set_decorated(GTK_WINDOW(win), FALSE);
    gtk_window_set_title(GTK_WINDOW(win), "penguin-esp");

    gtk_layer_init_for_window(GTK_WINDOW(win));
    if (!gtk_layer_is_supported()) {
        std::cerr << "ESP: layer-shell protocol not supported by compositor "
                  << "(make sure gtk4-layer-shell is installed and you are on Wayland)\n";
        gtk_window_destroy(GTK_WINDOW(win));
        return false;
    }

    gtk_layer_set_namespace(GTK_WINDOW(win), "sober-esp");
    gtk_layer_set_layer(GTK_WINDOW(win), GTK_LAYER_SHELL_LAYER_OVERLAY);
    for (auto edge : {GTK_LAYER_SHELL_EDGE_TOP, GTK_LAYER_SHELL_EDGE_BOTTOM,
                      GTK_LAYER_SHELL_EDGE_LEFT, GTK_LAYER_SHELL_EDGE_RIGHT})
        gtk_layer_set_anchor(GTK_WINDOW(win), edge, TRUE);
    gtk_layer_set_keyboard_mode(GTK_WINDOW(win), GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
    gtk_layer_set_exclusive_zone(GTK_WINDOW(win), -1);

    GdkDisplay* display = gtk_widget_get_display(win);
    GListModel* monitors = gdk_display_get_monitors(display);
    if (g_list_model_get_n_items(monitors) > 0) {
        GdkMonitor* mon = GDK_MONITOR(g_list_model_get_item(monitors, 0));
        GdkRectangle geom;
        gdk_monitor_get_geometry(mon, &geom);
        sw_ = geom.width;
        sh_ = geom.height;
    }

    GtkWidget* draw = gtk_drawing_area_new();
    layer_ = GtkLayerCtx{this, win, draw};
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(draw), draw_fn, &layer_, nullptr);
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(draw), sw_);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(draw), sh_);
    gtk_window_set_child(GTK_WINDOW(win), draw);
    apply_transparency_css(win, draw);

    gtk_ctx_ = g_main_context_ref_thread_default();
    gtk_ready_ = true;

    GMainLoop* loop = g_main_loop_new(nullptr, FALSE);
    g_object_set_data(G_OBJECT(win), "loop", loop);
    g_timeout_add(2, tick, &layer_);
    g_idle_add(on_ready_idle, &layer_);
    gtk_window_present(GTK_WINDOW(win));
    gtk_widget_queue_draw(draw);
    g_main_loop_run(loop);

    gtk_ready_ = false;
    g_main_loop_unref(loop);
    gtk_window_destroy(GTK_WINDOW(win));
    layer_ = {};
    return true;
}

void EspOverlay::set_crosshair_color(float r, float g, float b) {
    crosshair_.r.store(r, std::memory_order_relaxed);
    crosshair_.g.store(g, std::memory_order_relaxed);
    crosshair_.b.store(b, std::memory_order_relaxed);
}

std::tuple<float, float, float> EspOverlay::crosshair_color() const {
    return {crosshair_.r.load(std::memory_order_relaxed),
            crosshair_.g.load(std::memory_order_relaxed),
            crosshair_.b.load(std::memory_order_relaxed)};
}

void EspOverlay::set_fov_circle(float cx, float cy, float radius, float r, float g, float b, float a) {
    fov_.cx.store(cx, std::memory_order_relaxed);
    fov_.cy.store(cy, std::memory_order_relaxed);
    fov_.radius.store(radius, std::memory_order_relaxed);
    fov_.r.store(r, std::memory_order_relaxed);
    fov_.g.store(g, std::memory_order_relaxed);
    fov_.b.store(b, std::memory_order_relaxed);
    fov_.a.store(a, std::memory_order_relaxed);
    fov_.active.store(true, std::memory_order_relaxed);
}

void EspOverlay::set_fov_circle(float cx, float cy, float radius) {
    set_fov_circle(cx, cy, radius,
                   fov_.r.load(std::memory_order_relaxed),
                   fov_.g.load(std::memory_order_relaxed),
                   fov_.b.load(std::memory_order_relaxed),
                   fov_.a.load(std::memory_order_relaxed));
}

void EspOverlay::clear_fov_circle() {
    fov_.active.store(false, std::memory_order_relaxed);
}
