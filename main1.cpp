/******************************************************************************

                              Online C++ Compiler.
               Code, Compile, Run and Debug C++ program online.
Write your code in this editor and press "Run" button to compile and execute it.

*******************************************************************************/

#include <ncurses.h>
#include <cmath>
#include <ctime>
#include <unistd.h>
#include <signal.h>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <cstring>

// Forward declaration for the analog clock function from the other file
extern void init_analog_clock();

// --- Digital Clock Implementation ---

// Signal handler for window resize for the digital clock
static volatile sig_atomic_t resized_digital = 0;
void handle_winch_digital(int) {
    resized_digital = 1;
}

// Draws a horizontal or vertical line of blocks
void draw_segment(int y, int x, int len, bool is_horizontal) {
    if (is_horizontal) {
        for (int i = 0; i < len; ++i) {
            if (y >= 0 && y < LINES && (x + i) >= 0 && (x + i) < COLS) {
                 mvaddch(y, x + i, ACS_BLOCK);
            }
        }
    } else {
        for (int i = 0; i < len; ++i) {
            if ((y + i) >= 0 && (y + i) < LINES && x >= 0 && x < COLS) {
                mvaddch(y + i, x, ACS_BLOCK);
            }
        }
    }
}

// Draws a single digit using 7 segments, like an alarm clock
void draw_digit(int start_y, int start_x, int digit, int scale) {
    bool segments[7] = {false}; // top, middle, bottom, top-left, top-right, bottom-left, bottom-right

    switch (digit) {
        case 0: segments[0]=segments[2]=segments[3]=segments[4]=segments[5]=segments[6]=true; break;
        case 1: segments[4]=segments[6]=true; break;
        case 2: segments[0]=segments[1]=segments[2]=segments[4]=segments[5]=true; break;
        case 3: segments[0]=segments[1]=segments[2]=segments[4]=segments[6]=true; break;
        case 4: segments[1]=segments[3]=segments[4]=segments[6]=true; break;
        case 5: segments[0]=segments[1]=segments[2]=segments[3]=segments[6]=true; break;
        case 6: segments[0]=segments[1]=segments[2]=segments[3]=segments[5]=segments[6]=true; break;
        case 7: segments[0]=segments[4]=segments[6]=true; break;
        case 8: for(int i=0; i<7; ++i) segments[i]=true; break;
        case 9: segments[0]=segments[1]=segments[2]=segments[3]=segments[4]=segments[6]=true; break;
        default: break;
    }

    if (segments[0]) draw_segment(start_y, start_x + 1, scale, true); // top
    if (segments[1]) draw_segment(start_y + scale + 1, start_x + 1, scale, true); // middle
    if (segments[2]) draw_segment(start_y + 2 * scale + 2, start_x + 1, scale, true); // bottom
    if (segments[3]) draw_segment(start_y + 1, start_x, scale, false); // top-left
    if (segments[4]) draw_segment(start_y + 1, start_x + scale + 1, scale, false); // top-right
    if (segments[5]) draw_segment(start_y + scale + 2, start_x, scale, false); // bottom-left
    if (segments[6]) draw_segment(start_y + scale + 2, start_x + scale + 1, scale, false); // bottom-right
}

// Clears the area where a digit is drawn by filling it with spaces
void clear_digit(int start_y, int start_x, int scale) {
    int width = scale + 2;
    int height = 2 * scale + 3;
    for (int r = 0; r < height; ++r) {
        for (int c = 0; c < width; ++c) {
            if ((start_y + r) >= 0 && (start_y + r) < LINES && (start_x + c) >= 0 && (start_x + c) < COLS) {
                 mvaddch(start_y + r, start_x + c, ' ');
            }
        }
    }
}

// Draws the colon separators for the time
void draw_colon(int y, int x, int scale) {
    int colon_y_top = y + scale / 2;
    if (colon_y_top < y + 1) colon_y_top = y + 1;
    int colon_y_bottom = y + scale + 2 + scale / 2;
    
    mvaddch(colon_y_top, x, 'o');
    mvaddch(colon_y_bottom, x, 'o');
}

// Main function to run the digital clock display
void init_digital_clock() {
    clear();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);

    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(4, COLOR_CYAN, -1);
    }
    
    signal(SIGWINCH, handle_winch_digital);

    // Store previous time to only update changed digits
    struct tm prev_tmnow = { .tm_sec = -1 }; // Initialize to force first draw
    int last_scale = -1; // Store last scale to detect resize changes

    while (true) {
        if (resized_digital) {
            resized_digital = 0;
            endwin();
            refresh();
            // Setting last_scale to -1 will force a full clear and redraw
            last_scale = -1; 
        }

        int max_y, max_x;
        getmaxyx(stdscr, max_y, max_x);
        
        // --- Calculate scale and positions ---
        // A digit is (scale+2) wide and (2*scale+3) high
        int scale = std::max(1, std::min((max_y - 3) / 2, (max_x - 10) / 8));
        
        // A change in scale (from resize) forces a full redraw
        bool force_redraw = (last_scale != scale);
        if (force_redraw) {
            clear();
            last_scale = scale;
            prev_tmnow.tm_sec = -1; // Trigger redraw of all time components
        }
        
        int digit_w = scale + 2;
        int digit_h = 2 * scale + 3;
        int colon_spacing = scale > 1 ? 3 : 2;

        int total_w = (6 * digit_w) + (2 * colon_spacing);

        if (max_y < digit_h || max_x < total_w) {
            if (!force_redraw) clear(); // Clear previous clock if it becomes too small
            mvprintw(max_y / 2, (max_x - 20) / 2, "Terminal too small");
            refresh();
            int ch = getch();
            if (ch == 'q' || ch == 'Q' || ch == KEY_UP) break;
            usleep(200000);
            last_scale = -1; // Force redraw after coming back
            continue;
        }

        int start_x = (max_x - total_w) / 2;
        int start_y = (max_y - digit_h) / 2;

        time_t now = time(0);
        struct tm* tmnow = localtime(&now);
        
        bool is_first_draw = (prev_tmnow.tm_sec == -1);

        if (has_colors()) attron(COLOR_PAIR(4));
        
        int current_x = start_x;
        
        // Hour
        if (is_first_draw || tmnow->tm_hour != prev_tmnow.tm_hour) {
            if (!is_first_draw) {
                clear_digit(start_y, current_x, scale);
                clear_digit(start_y, current_x + digit_w, scale);
            }
            draw_digit(start_y, current_x, tmnow->tm_hour / 10, scale);
            draw_digit(start_y, current_x + digit_w, tmnow->tm_hour % 10, scale);
        }
        current_x += 2 * digit_w;

        // Colon 1
        if (is_first_draw) {
             draw_colon(start_y, current_x + colon_spacing / 2, scale);
        }
        current_x += colon_spacing;

        // Minute
        if (is_first_draw || tmnow->tm_min != prev_tmnow.tm_min) {
            if (!is_first_draw) {
                clear_digit(start_y, current_x, scale);
                clear_digit(start_y, current_x + digit_w, scale);
            }
            draw_digit(start_y, current_x, tmnow->tm_min / 10, scale);
            draw_digit(start_y, current_x + digit_w, tmnow->tm_min % 10, scale);
        }
        current_x += 2 * digit_w;
        
        // Colon 2
        if (is_first_draw) {
            draw_colon(start_y, current_x + colon_spacing / 2, scale);
        }
        current_x += colon_spacing;

        // Second
        if (is_first_draw || tmnow->tm_sec != prev_tmnow.tm_sec) {
            if (!is_first_draw) {
                clear_digit(start_y, current_x, scale);
                clear_digit(start_y, current_x + digit_w, scale);
            }
            draw_digit(start_y, current_x, tmnow->tm_sec / 10, scale);
            draw_digit(start_y, current_x + digit_w, tmnow->tm_sec % 10, scale);
        }
        
        prev_tmnow = *tmnow; // Update previous time

        if (has_colors()) attroff(COLOR_PAIR(4));
        refresh();

        int ch = getch();
        if (ch == 'q' || ch == 'Q' || ch == KEY_UP) break;
        
        usleep(200000);
    }
    nodelay(stdscr, FALSE); // Go back to blocking mode for the menu
}

int main() {
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    
    int ch;
    do {
        clear();
        refresh();
        
        ch = getch(); // Wait for user input

        switch (ch) {
            case KEY_UP:
                init_analog_clock();
                break;
            case KEY_DOWN:
                init_digital_clock();
                break;
        }
    } while (ch != 'q' && ch != 'Q');

    endwin();
    return 0;
}
