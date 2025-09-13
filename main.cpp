#include <ncurses.h>
#include <cmath>
#include <ctime>
#include <unistd.h>
#include <signal.h>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <cstring>

static volatile sig_atomic_t resized = 0;

void handle_winch(int) { resized = 1; }

// A struct to hold a point's coordinates
struct Point { int y, x; };

// Function to draw the static parts of the clock (border and marks)
void draw_static_clock(int cy, int cx, int radius) {
    // Draw circle-like border (approx) using 'o'
    attron(COLOR_PAIR(1));
    for (int angle = 0; angle < 360; ++angle) {
        double rad = angle * M_PI / 180.0;
        int y = cy + static_cast<int>(round(sin(rad) * radius));
        // Adjust x-scaling to account for character aspect ratio
        int x = cx + static_cast<int>(round(cos(rad) * radius * 2.0 / 2.0)); 
        if (y >= 0 && y < LINES && x >= 0 && x < COLS) {
            mvaddch(y, x, 'o');
        }
    }
    attroff(COLOR_PAIR(1));

    // Draw hour marks (12 marks)
    attron(COLOR_PAIR(3));
    for (int h = 0; h < 12; ++h) {
        // Correctly offset the angle so that 12 is at the top (-PI/2)
        double ang = (h / 12.0) * 2.0 * M_PI - M_PI/2;
        int ry = cy + static_cast<int>(round(sin(ang) * (radius - 1.5)));
        int rx = cx + static_cast<int>(round(cos(ang) * (radius - 1.5)));

        // Convert hour value to a string
        char hour_str[3];
        int hour = (h == 0) ? 12 : h;
        snprintf(hour_str, sizeof(hour_str), "%d", hour);

        // Adjust x-coordinate to center the number
        int len = strlen(hour_str);
        rx -= (len / 2);

        if (ry >= 0 && ry < LINES && rx >= 0 && rx + len < COLS) {
            mvprintw(ry, rx, "%s", hour_str);
        }
    }
    attroff(COLOR_PAIR(3));
}

// Function to draw a hand and store its points in a vector
// The hand character is determined by its angle
void draw_hand(double angle, int length, int cy, int cx, std::vector<Point>& points) {
    points.clear();
    attron(COLOR_PAIR(2));

    double deg = angle * 180.0 / M_PI;
    while (deg < 0) deg += 360.0;
    while (deg >= 360.0) deg -= 360.0;

    for (int r = 1; r <= length; ++r) {
        double fy = cy + sin(angle) * r;
        double fx = cx + cos(angle) * r * 0.5;
        int iy = static_cast<int>(round(fy));
        int ix = static_cast<int>(round(fx));

        if (iy < 0 || iy >= LINES || ix < 0 || ix >= COLS) break;

        chtype use_char;
        if ((deg >= 350 || deg < 10) || (deg >= 170 && deg < 190) || (deg >= 80 && deg < 100) || (deg >= 260 && deg < 280)) {
            use_char = '|';
        } else if ((deg > 10 && deg < 80) || (deg > 190 && deg < 260)) {
            use_char = '/';
        } else {
            use_char = '\\';
        }

        mvaddch(iy, ix, use_char);
        points.push_back({iy, ix});
    }
    attroff(COLOR_PAIR(2));
}

// Function to clear a hand by drawing spaces over its previous points
void clear_hand(const std::vector<Point>& points) {
    for (const auto& p : points) {
        mvaddch(p.y, p.x, ' ');
    }
}

int main() {
    initscr();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    start_color();
    use_default_colors();
    init_pair(1, COLOR_CYAN, -1);
    init_pair(2, COLOR_YELLOW, -1);
    init_pair(3, COLOR_WHITE, -1);

    signal(SIGWINCH, handle_winch);

    // Initial draw of the static clock components
    int maxy, maxx;
    getmaxyx(stdscr, maxy, maxx);
    int radius = std::min(maxy / 2, maxx / 2) - 2;
    int cy = maxy / 2;
    int cx = maxx / 2;
    
    // Check if terminal is too small on startup
    if (radius < 3) {
        clear();
        mvprintw(maxy / 2, (maxx - 20) / 2, "Terminal too small");
        refresh();
    } else {
        draw_static_clock(cy, cx, radius);
    }
    
    // Vectors to store the previous hand positions
    std::vector<Point> prev_hour_points;
    std::vector<Point> prev_min_points;
    std::vector<Point> prev_sec_points;

    while (true) {
        // Handle window resize event
        if (resized) {
            resized = 0;
            // Clear previous points and redraw the entire static clock
            prev_hour_points.clear();
            prev_min_points.clear();
            prev_sec_points.clear();
            endwin();
            refresh();
            clear();
            getmaxyx(stdscr, maxy, maxx);
            radius = std::min(maxy / 2, maxx / 2) - 2;
            cy = maxy / 2;
            cx = maxx / 2;
            if (radius < 3) {
                mvprintw(maxy / 2, (maxx - 20) / 2, "Terminal too small");
            } else {
                draw_static_clock(cy, cx, radius);
            }
        }

        if (radius < 3) {
            // Wait for resize
            int ch = getch();
            if (ch == 'q' || ch == 'Q') break;
            usleep(200000);
            continue;
        }

        // Clear old hands
        clear_hand(prev_hour_points);
        clear_hand(prev_min_points);
        clear_hand(prev_sec_points);

        // Get current time
        timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        time_t now = ts.tv_sec;
        struct tm tmnow;
        localtime_r(&now, &tmnow);

        double sec = tmnow.tm_sec + ts.tv_nsec / 1e9;
        double min = tmnow.tm_min + sec / 60.0;
        double hour = (tmnow.tm_hour % 12) + min / 60.0;

        // Angles: -pi/2 (12 o'clock) + clockwise
        double ang_sec = (sec / 60.0) * 2.0 * M_PI - M_PI/2;
        double ang_min = (min / 60.0) * 2.0 * M_PI - M_PI/2;
        double ang_hour = (hour / 12.0) * 2.0 * M_PI - M_PI/2;

        // Draw new hands and store their points
        draw_hand(ang_hour, std::max(1, radius * 2 / 5), cy, cx, prev_hour_points);
        draw_hand(ang_min, std::max(1, radius * 3 / 5), cy, cx, prev_min_points);
        draw_hand(ang_sec, std::max(1, radius - 2), cy, cx, prev_sec_points);

        // Center
        attron(COLOR_PAIR(3));
        mvaddch(cy, cx, 'O');
        attroff(COLOR_PAIR(3));
        
        // Draw digital time below clock if space permits
        char timestr[64];
        snprintf(timestr, sizeof(timestr), "%02d:%02d:%02d", tmnow.tm_hour, tmnow.tm_min, tmnow.tm_sec);
        int tlen = static_cast<int>(strlen(timestr));
        mvprintw(std::min(maxy - 1, cy + radius + 1), std::max(0, cx - tlen / 2), "%s", timestr);

        refresh();

        // Handle input: allow quit
        int ch = getch();
        if (ch == 'q' || ch == 'Q') break;

        // Sleep to control update rate
        usleep(200000);
    }

    endwin();
    return 0;
}
