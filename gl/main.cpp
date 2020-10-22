#include <GLFW/glfw3.h>
#include <cstdlib>
#include <iostream>
#include <cstdlib>
#include <vector>
#include <chrono>
#include <algorithm>


struct color {
    GLbyte r, g, b;
};

struct point2d {
    double x, y;
};

bool operator==(const point2d& a, const point2d& b) {
    return a.x == b.x && a.y == b.y;
}
bool operator!=(const point2d& a, const point2d& b) {
    return !(a == b);
}
point2d operator-(const point2d& a, const point2d& b) {
    return { a.x - b.x, a.y - b.y };
}

point2d operator+(const point2d& a, const point2d& b) {
    return { a.x + b.x, a.y + b.y };
}

point2d operator*(double k, const point2d& a) {
    return { k * a.x, k *  a.y };
}
double operator*(const point2d& a, const point2d& b) {
    return a.x * b.x + a.y * b.y;
}
double dot(const point2d& a, const point2d& b) {
    return a * b;
}
double abs2(const point2d& a) {
    return a * a;
}
double abs(const point2d& a) {
    return sqrt(abs2(a));
}
double dist2(const point2d& a, const point2d& b) {
    return abs2(b - a);
}
double dist(const point2d& a, const point2d& b) {
    return sqrt(dist2(a, b));
}
double dist2(const point2d& p, const point2d& v, const point2d& w) {
    auto l2 = dist2(v, w);
    if (l2 == 0.0) return dist2(p, v);
    auto t = std::clamp(dot(p - v, w - v) / l2, 0.0, 1.0);
    auto projection = v + t * (w - v);
    return dist2(p, projection);
}
point2d rotate(const point2d& a) {
    return { a.y, -a.x };
}

using polyline = std::vector<point2d>;

void split(const point2d& p0, const point2d& p1, const polyline& pattern, int iterations, double max_len, polyline& result) {
    if (iterations == 0) return;

    auto a = pattern.front();
    auto b = pattern.back();
    auto ab = b - a;
    auto len = abs2(ab);
    if (len < 1e-15) return;
    auto scale = 1 / len;
    auto ort_x = ab;
    auto ort_y = rotate(ort_x);

    auto target_x = p1 - p0;
    auto target_y = rotate(target_x);

    auto transform = [&](const point2d& p) {
        auto v = p - a;
        auto x = v * ort_x * scale;
        auto y = v * ort_y * scale;
        return p0 + x * target_x + y * target_y;
    };

    point2d prev;
    for (auto& p : pattern) {
        auto i = (size_t)(&p - &pattern[0]);
        auto transformed = transform(p);
        if (i > 0 && dist2(pattern[i - 1], p) < max_len) {
            split(prev, transformed, pattern, iterations - 1, max_len, result);
            if (i < pattern.size() - 1) {
                result.push_back(transformed);
            }
        }
        prev = transformed;
    }
}

polyline build_fractal(const polyline& pattern, int iterations) {

    if (pattern.size() < 2) return{};

    auto max_len = 0.9 * dist2(pattern.front(), pattern.back());

    polyline result;
    result.reserve((size_t)round(pow(pattern.size() - 1, iterations)) + 1);

    for (auto& p : pattern) {
        auto i = &p - &pattern[0];
        if (i > 0 && dist2(pattern[i - 1], p) < max_len) {
            split(pattern[i - 1], p, pattern, iterations - 1, max_len, result);
        }
        result.push_back(p);
    }
    return result;
}

polyline build_fractal(const polyline& pattern) {
    int iterations = 1;
    while (pow(pattern.size() - 1, iterations) < 2000)
        iterations++;
    return build_fractal(pattern, iterations);
}

void split_largest(polyline& fractal, polyline& part, const polyline& pattern) {
    double max = 0;
    size_t max_i = 0;
    for (auto& p : fractal) {
        auto i = &p - &fractal[0];
        if (i == 0) continue;
        auto l = dist2(p, fractal[i - 1]);
        if (l > max) { max = l; max_i = i; }
    }
    part.clear();
    part.reserve(pattern.size() - 2);
    split(fractal[max_i - 1], fractal[max_i], pattern, 1, 1e10, part);
    fractal.insert(fractal.begin() + max_i, part.begin(), part.end());
}

void split_largest_2(polyline& fractal, const polyline& pattern) {
    double max = 0;
    for (auto& p : fractal) {
        auto i = &p - &fractal[0];
        if (i == 0) continue;
        auto l = dist2(p, fractal[i - 1]);
        if (l > max) max = l;
    }
    max *= 0.95;
    size_t capacity = fractal.size();
    for (auto& p : fractal) {
        auto i = &p - &fractal[0];
        if (i == 0) continue;
        auto l = dist2(p, fractal[i - 1]);
        if (l > max) {
            capacity += pattern.size() - 2;
        }
    }
    if (capacity > 1'000'000) return;
    polyline part;
    part.reserve(capacity);
    for (auto& p : fractal) {
        auto i = &p - &fractal[0];
        if (i > 0) {
            auto l = dist2(p, fractal[i - 1]);
            if (l > max) {
                split(fractal[i - 1], p, pattern, 1, 1e10, part);;
            }
        }
        part.push_back(p);
    }
    swap(fractal, part);
}

polyline pattern;
polyline fractal;

size_t selected_vertex = -1;
size_t selected_segment = -1;
point2d mouse_pos;
bool left_mouse_button_pressed = false;
bool right_mouse_button_pressed = false;
double base_color = 1.0;

void update_pattern() {
    bool update_needed = false;
    if (left_mouse_button_pressed) {
        if (selected_vertex < pattern.size()) {
            update_needed |= pattern[selected_vertex] != mouse_pos;
            pattern[selected_vertex] = mouse_pos;
        }
        if (selected_segment < pattern.size()) {
            update_needed = true;
            pattern.insert(pattern.begin() + selected_segment, mouse_pos);
        }
    }
    if (right_mouse_button_pressed) {
        if (selected_vertex < pattern.size()) {
            update_needed = true;
            pattern.erase(pattern.begin() + selected_vertex);
        }
    }
    if (update_needed){
        fractal = build_fractal(pattern);
        for (int i=0; i<10; ++i)
            split_largest_2(fractal, pattern);
    }
}

void process_mouse_move()
{
    update_pattern();

    selected_vertex = -1;
    selected_segment = -1;

    auto size = 5.0;
    for (auto& p : pattern)
    {
        if (dist2(p, mouse_pos) <= size * size) {
            selected_vertex = &p - &pattern[0];
            goto update_positions;
        }
    }
    for (auto& p : pattern)
    {
        auto i = &p - &pattern[0];
        if (i == 0) continue;

        if (dist2(mouse_pos, p, pattern[i - 1]) <= size * size) {
            selected_segment = i;
            goto update_positions;
        }
    }

update_positions:
    update_pattern();
}

const auto screen_width = 1800;
const auto screen_height = 900;

void on_mouse_move(GLFWwindow* window, double xpos, double ypos) {
    mouse_pos = { xpos , ypos };
    base_color = 1.0;
    process_mouse_move();
}

void on_mouse_button(GLFWwindow* window, int button, int action, int mods) {
    switch (button) {
    case GLFW_MOUSE_BUTTON_LEFT:
        left_mouse_button_pressed = action == GLFW_PRESS;
        break;
    case GLFW_MOUSE_BUTTON_RIGHT:
        right_mouse_button_pressed = action == GLFW_PRESS;
        break;
    }
    process_mouse_move();
}

void on_key_pressed(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        switch (key) {
        case GLFW_KEY_SPACE:
        {
            polyline part;
            for (int i = 0; i < 1000; ++i)
                split_largest(fractal, part, pattern);
            return;
        }
        case GLFW_KEY_ESCAPE:
        {
            exit(0);
        }
        }
    }
}

int main(void)
{
    if (!glfwInit())
        return -1;

    // Create a windowed mode window and its OpenGL context
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    GLFWwindow* window = glfwCreateWindow(screen_width, screen_height, "Hello World", 0/*monitor*/, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    // Make the window's context current
    glfwMakeContextCurrent(window);

    // Enable v-sync
    glfwSwapInterval(1);

    glfwSetCursorPosCallback(window, &on_mouse_move);
    glfwSetMouseButtonCallback(window, &on_mouse_button);
    glfwSetKeyCallback(window, &on_key_pressed);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_POINT_SMOOTH);
    glEnable(GL_LINE_SMOOTH);

    glOrtho(0.0, screen_width, screen_height, 0, -1, 1);

    // Initil setup:
    pattern = { {80, 600},{400, 600}, {900, 400}, {1400, 600}, {1720, 600} };
    fractal = build_fractal(pattern);

    while (!glfwWindowShouldClose(window))
    {
        glClear(GL_COLOR_BUFFER_BIT);

        // Selected segment
        if (selected_segment < pattern.size()) {
            glLineWidth(5);
            glColor3b(20, 20, 100);
            glBegin(GL_LINE_STRIP);
            glVertex2dv(&pattern[selected_segment - 1].x);
            glVertex2dv(&pattern[selected_segment].x);
            glEnd();
        }

        // Fractal
        glColor3b(100, 100, 100);
        glLineWidth(0.2f);
        glBegin(GL_LINE_STRIP);
        for (auto& p : fractal)
            glVertex2dv(&p.x);
        glEnd();


        // Pattern
        glColor4d(0.2, 0.7, 0.3, base_color);
        glLineWidth(3.0);
        glBegin(GL_LINE_STRIP);
        for (auto& p : pattern)
            glVertex2dv(&p.x);
        glEnd();

        // Selected node
        if (selected_vertex < pattern.size()) {
            glPointSize(15);
            glColor3b(20, 20, 100);
            glBegin(GL_POINTS);
            glVertex2dv(&pattern[selected_vertex].x);
            glEnd();
        }

        // Swap front and back buffers
        glfwSwapBuffers(window);

        // Poll for and process events
        glfwPollEvents();

        // Grow fractal
        split_largest_2(fractal, pattern);

        // Fade pattern
        base_color *= 0.95;
    }

    glfwTerminate();
    return 0;
}