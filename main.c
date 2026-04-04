#include <gtk/gtk.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

static GtkWidget *entry[4];
static GtkComboBoxText *quality_combo;
static GtkWidget *folder_label;
static GtkWidget *log_textview;
static GtkTextBuffer *log_buffer;
static char *save_path = NULL;
static char config_path[1024];

// Функция для добавления сообщения в лог
static void add_to_log(const char *message) {
    GtkTextIter iter;
    time_t now;
    struct tm *tm_info;
    char time_str[64];
    
    time(&now);
    tm_info = localtime(&now);
    strftime(time_str, sizeof(time_str), "[%H:%M:%S] ", tm_info);
    
    gtk_text_buffer_get_end_iter(log_buffer, &iter);
    gtk_text_buffer_insert(log_buffer, &iter, time_str, -1);
    gtk_text_buffer_insert(log_buffer, &iter, message, -1);
    gtk_text_buffer_insert(log_buffer, &iter, "\n", -1);
}

// Загрузка конфига
static void load_config() {
    snprintf(config_path, sizeof(config_path), "%s/.duckloader_config", getenv("HOME"));
    FILE *f = fopen(config_path, "r");
    if (f) {
        char line[1024];
        if (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\n")] = 0;
            if (strlen(line) > 0 && access(line, F_OK) == 0) {
                save_path = strdup(line);
                gtk_label_set_text(GTK_LABEL(folder_label), save_path);
                add_to_log("📂 Загружена сохранённая папка ✅");
            }
        }
        fclose(f);
    }
}

// Сохранение конфига
static void save_config() {
    if (save_path) {
        FILE *f = fopen(config_path, "w");
        if (f) {
            fprintf(f, "%s\n", save_path);
            fclose(f);
        }
    }
}

// Очистка всех полей
static void clear_fields(GtkWidget *widget, gpointer data) {
    for (int i = 0; i < 4; i++) {
        gtk_entry_set_text(GTK_ENTRY(entry[i]), "");
    }
    add_to_log("🧹 Поля очищены ✨");
}

static void choose_folder(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Выберите папку для сохранения",
                                                    NULL,
                                                    GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                                    "_Отмена",
                                                    GTK_RESPONSE_CANCEL,
                                                    "_Выбрать",
                                                    GTK_RESPONSE_ACCEPT,
                                                    NULL);
    
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *folder = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (save_path) free(save_path);
        save_path = strdup(folder);
        gtk_label_set_text(GTK_LABEL(folder_label), folder);
        save_config();
        add_to_log("📁 Выбрана папка: ");
        add_to_log(folder);
        g_free(folder);
    }
    
    gtk_widget_destroy(dialog);
}

// Проверка наличия yt-dlp
static int check_yt_dlp() {
    int result = system("which yt-dlp > /dev/null 2>&1");
    if (result != 0) {
        GtkWidget *dialog = gtk_message_dialog_new(NULL,
                                                    GTK_DIALOG_MODAL,
                                                    GTK_MESSAGE_ERROR,
                                                    GTK_BUTTONS_OK,
                                                    "yt-dlp не установлен!\n\n"
                                                    "🔧 Установите его:\n"
                                                    "• 🐧 Ubuntu/Mint: sudo apt install yt-dlp\n"
                                                    "• 🐧 Fedora: sudo dnf install yt-dlp\n"
                                                    "• 🐧 Arch: sudo pacman -S yt-dlp\n"
                                                    "• 📦 Другие: pipx install yt-dlp");
        gtk_window_set_title(GTK_WINDOW(dialog), "⛔ Ошибка");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return 0;
    }
    return 1;
}

// Проверка версии yt-dlp (должна быть не ниже 2025)
static int check_yt_dlp_version() {
    char buffer[128];
    FILE *fp = popen("yt-dlp --version", "r");
    if (!fp) return 1;
    
    if (fgets(buffer, sizeof(buffer), fp) != NULL) {
        pclose(fp);
        
        int year = 0;
        sscanf(buffer, "%d", &year);
        
        if (year < 2025) {
            GtkWidget *dialog = gtk_message_dialog_new(NULL,
                                                        GTK_DIALOG_MODAL,
                                                        GTK_MESSAGE_WARNING,
                                                        GTK_BUTTONS_OK,
                                                        "⚠️ yt-dlp устарел!\n\n"
                                                        "Ваша версия: %s\n"
                                                        "Рекомендуется версия 2025 или новее.\n\n"
                                                        "🔧 Обновите его:\n"
                                                        "• pipx upgrade yt-dlp\n"
                                                        "• Или: sudo apt update && sudo apt install yt-dlp\n\n"
                                                        "Иначе некоторые видео могут не скачиваться.", buffer);
            gtk_window_set_title(GTK_WINDOW(dialog), "⚠️ Рекомендация");
            gtk_dialog_run(GTK_DIALOG(dialog));
            gtk_widget_destroy(dialog);
            return 0;
        }
    } else {
        pclose(fp);
    }
    return 1;
}

// Проверка наличия ffmpeg
static int check_ffmpeg() {
    int result = system("which ffmpeg > /dev/null 2>&1");
    if (result != 0) {
        GtkWidget *dialog = gtk_message_dialog_new(NULL,
                                                    GTK_DIALOG_MODAL,
                                                    GTK_MESSAGE_WARNING,
                                                    GTK_BUTTONS_OK,
                                                    "🔧 ffmpeg не установлен!\n\n"
                                                    "ffmpeg нужен для объединения видео и аудио.\n\n"
                                                    "Установите его:\n"
                                                    "• 🐧 Ubuntu/Mint: sudo apt install ffmpeg\n"
                                                    "• 🐧 Fedora: sudo dnf install ffmpeg\n"
                                                    "• 🐧 Arch: sudo pacman -S ffmpeg");
        gtk_window_set_title(GTK_WINDOW(dialog), "⚠️ Предупреждение");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return 0;
    }
    return 1;
}

static void download_clicked(GtkWidget *widget, gpointer data) {
    char command[2048];
    char full_command[8192] = "";
    char cd_command[1024] = "";
    int has_urls = 0;
    
    if (!check_yt_dlp()) return;
    if (!check_yt_dlp_version()) return;
    if (!check_ffmpeg()) return;
    
    if (save_path) {
        snprintf(cd_command, sizeof(cd_command), "cd \"%s\" && ", save_path);
    }
    
    for (int i = 0; i < 4; i++) {
        const char *url = gtk_entry_get_text(GTK_ENTRY(entry[i]));
        
        if (url && url[0] != '\0') {
            has_urls = 1;
            const char *quality = gtk_combo_box_text_get_active_text(quality_combo);
            
            if (strcmp(quality, "🌟 Лучшее (макс. качество)") == 0) {
                snprintf(command, sizeof(command), "%s yt-dlp -f \"bestvideo+bestaudio\" --merge-output-format mp4 \"%s\"", cd_command, url);
            } else if (strcmp(quality, "🌝 1080p (Full HD)") == 0) {
                snprintf(command, sizeof(command), "%s yt-dlp -f \"bestvideo[height<=1080]+bestaudio\" --merge-output-format mp4 \"%s\"", cd_command, url);
            } else if (strcmp(quality, "🌗 720p (HD)") == 0) {
                snprintf(command, sizeof(command), "%s yt-dlp -f \"bestvideo[height<=720]+bestaudio\" --merge-output-format mp4 \"%s\"", cd_command, url);
            } else if (strcmp(quality, "🌜 480p") == 0) {
                snprintf(command, sizeof(command), "%s yt-dlp -f \"bestvideo[height<=480]+bestaudio\" --merge-output-format mp4 \"%s\"", cd_command, url);
            } else if (strcmp(quality, "🌚 360p") == 0) {
                snprintf(command, sizeof(command), "%s yt-dlp -f \"bestvideo[height<=360]+bestaudio\" --merge-output-format mp4 \"%s\"", cd_command, url);
            } else if (strcmp(quality, "🌏 Только аудио (MP3)") == 0) {
                snprintf(command, sizeof(command), "%s yt-dlp -x --audio-format mp3 \"%s\"", cd_command, url);
            }
            
            strcat(full_command, command);
            strcat(full_command, " && ");
            add_to_log("📩 Начата загрузка URL");
        }
    }
    
    if (has_urls) {
        full_command[strlen(full_command) - 4] = '\0';
        
        GdkCursor *cursor = gdk_cursor_new_for_display(gdk_display_get_default(), GDK_WATCH);
        gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(widget)), cursor);
        g_object_unref(cursor);
        
        while (gtk_events_pending()) gtk_main_iteration();
        
        system(full_command);
        
        cursor = gdk_cursor_new_for_display(gdk_display_get_default(), GDK_LEFT_PTR);
        gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(widget)), cursor);
        g_object_unref(cursor);
        
        add_to_log("🚛 Все загрузки завершены!");
        
        system("paplay duckloader.wav 2>/dev/null || aplay duckloader.wav 2>/dev/null || echo -e '\a'");
        
        GtkWidget *dialog = gtk_message_dialog_new(NULL,
                                                    GTK_DIALOG_MODAL,
                                                    GTK_MESSAGE_INFO,
                                                    GTK_BUTTONS_OK,
                                                    "🥳 Загрузка всех видео завершена!");
        gtk_window_set_title(GTK_WINDOW(dialog), "🦆 Кря!");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
    }
}

// Диалог "О программе" — выезжающее окно (Popover)
static void show_about(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    GtkWidget *popover;
    GtkWidget *vbox;
    GtkWidget *label;
    
    popover = gtk_popover_new(widget);
    gtk_popover_set_position(GTK_POPOVER(popover), GTK_POS_BOTTOM);
    
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(popover), vbox);
    
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), 
        "<span size=\"large\"><b>🦆 Качка [Duckloader] 📦</b></span>\n"
        "<span size=\"small\">версия 0.1 — сонная утка</span>\n"
        "<span size=\"small\">автор: VibeCoder0 &amp; AI</span>\n\n"
        "Здравствуйте🖖, я не программист🧑‍💻,\n"
        "но мне нужен был простой способ\n"
        "скачивать видео с YouTube🟥 на Linux🐧.\n"
        "Готовые программы из менеджера приложений🟩\n"
        "по каким-то причинам перестали работать❎.\n"
        "Тогда я решил написать🧑‍🎨 свою программу —\n" 
        "вместе с нейросетью🤖.\n"
        "P.S. Качка - это утка...или\n"
        "от слова 'качать', я сам не знаю 😁");
    
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
    
    gtk_widget_set_margin_top(label, 10);
    gtk_widget_set_margin_bottom(label, 10);
    gtk_widget_set_margin_start(label, 10);
    gtk_widget_set_margin_end(label, 10);
    
    gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);
    
    gtk_widget_show_all(popover);
}

// ==================== ОТКРЫТИЕ ПАПКИ ЗАГРУЗКИ ====================
static void open_folder(GtkWidget *widget, gpointer data) {
    if (save_path && save_path[0] != '\0') {
        char command[1024];
        snprintf(command, sizeof(command), "xdg-open \"%s\"", save_path);
        system(command);
    } else {
        add_to_log("⚠️ Папка не выбрана");
    }
}

int main(int argc, char *argv[]) {
    GtkWidget *window;
    GtkWidget *vbox;
    GtkWidget *button;
    GtkWidget *clear_button;
    GtkWidget *folder_button;
    GtkWidget *label;
    GtkWidget *scrolled_window;
    GtkWidget *button_box;
    GtkWidget *menubar;
    GtkWidget *about_item;
    
    gtk_init(&argc, &argv);
    
    // ==================== ОПРЕДЕЛЯЕМ ПУТЬ К ФОНУ ====================
    char background_path[1024];
    FILE *test_file = fopen("background.png", "r");
    if (test_file) {
        fclose(test_file);
        strcpy(background_path, "background.png");
    } else {
        strcpy(background_path, "/usr/local/share/duckloader/background.png");
    }
    // ===============================================================
    
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "🦆 Качка [Duckloader] 📦");
    gtk_window_set_default_size(GTK_WINDOW(window), 700, 600);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    
    menubar = gtk_menu_bar_new();
    about_item = gtk_menu_item_new_with_label("О программе 📺");
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), about_item);
    g_signal_connect(about_item, "button-press-event", G_CALLBACK(show_about), NULL);
    
    GdkPixbuf *icon = gdk_pixbuf_new_from_file("duckloader.png", NULL);
    if (icon) {
        gtk_window_set_icon(GTK_WINDOW(window), icon);
        g_object_unref(icon);
    }
    
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    
    gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, FALSE, 0);
    
    // ==================== ПОЛЕ 1 ====================
    entry[0] = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry[0]), "Вставьте 🦆 ссылку №1");
    gtk_widget_set_margin_top(entry[0], 3);
    gtk_widget_set_margin_bottom(entry[0], 3);
    gtk_widget_set_margin_start(entry[0], 15);
    gtk_widget_set_margin_end(entry[0], 15);
    gtk_box_pack_start(GTK_BOX(vbox), entry[0], FALSE, FALSE, 0);
    
    // ==================== ПОЛЕ 2 ====================
    entry[1] = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry[1]), "Вставьте 🦆 ссылку №2");
    gtk_widget_set_margin_top(entry[1], 3);
    gtk_widget_set_margin_bottom(entry[1], 3);
    gtk_widget_set_margin_start(entry[1], 45);
    gtk_widget_set_margin_end(entry[1], 45);
    gtk_box_pack_start(GTK_BOX(vbox), entry[1], FALSE, FALSE, 0);
    
    // ==================== ПОЛЕ 3 ====================
    entry[2] = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry[2]), "Вставьте 🦆 ссылку №3");
    gtk_widget_set_margin_top(entry[2], 3);
    gtk_widget_set_margin_bottom(entry[2], 3);
    gtk_widget_set_margin_start(entry[2], 45);
    gtk_widget_set_margin_end(entry[2], 45);
    gtk_box_pack_start(GTK_BOX(vbox), entry[2], FALSE, FALSE, 0);
    
    // ==================== ПОЛЕ 4 ====================
    entry[3] = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry[3]), "Вставьте 🦆 ссылку №4");
    gtk_widget_set_margin_top(entry[3], 3);
    gtk_widget_set_margin_bottom(entry[3], 3);
    gtk_widget_set_margin_start(entry[3], 15);
    gtk_widget_set_margin_end(entry[3], 15);
    gtk_box_pack_start(GTK_BOX(vbox), entry[3], FALSE, FALSE, 0);
    
    // ==================== КНОПКИ ====================
    button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    
    clear_button = gtk_button_new_with_label("🧹 Очистить поля");
    gtk_widget_set_name(clear_button, "clear-button");
    g_signal_connect(clear_button, "clicked", G_CALLBACK(clear_fields), NULL);
    gtk_widget_set_margin_top(clear_button, 3);
    gtk_widget_set_margin_bottom(clear_button, 3);
    gtk_widget_set_margin_start(clear_button, 90);
    gtk_widget_set_margin_end(clear_button, 15);
    
    folder_button = gtk_button_new_with_label("📁 Выбрать папку");
    gtk_widget_set_name(folder_button, "folder-button");
    g_signal_connect(folder_button, "clicked", G_CALLBACK(choose_folder), NULL);
    gtk_widget_set_margin_top(folder_button, 3);
    gtk_widget_set_margin_bottom(folder_button, 3);
    gtk_widget_set_margin_start(folder_button, 15);
    gtk_widget_set_margin_end(folder_button, 90);
    
    gtk_box_pack_start(GTK_BOX(button_box), clear_button, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(button_box), folder_button, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), button_box, FALSE, FALSE, 5);
    
    // ==================== МЕТКА ПАПКИ ====================
    folder_label = gtk_label_new("Текущая папка (по умолчанию)");
    gtk_widget_set_margin_top(folder_label, 3);
    gtk_widget_set_margin_bottom(folder_label, 3);
    gtk_widget_set_margin_start(folder_label, 90);
    gtk_widget_set_margin_end(folder_label, 90);
    gtk_box_pack_start(GTK_BOX(vbox), folder_label, FALSE, FALSE, 0);
    
    // ==================== МЕТКА КАЧЕСТВА ====================
    label = gtk_label_new("Качество (для всех видео):");
    gtk_widget_set_margin_top(label, 3);
    gtk_widget_set_margin_bottom(label, 3);
    gtk_widget_set_margin_start(label, 90);
    gtk_widget_set_margin_end(label, 90);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
    
    // ==================== ВЫПАДАЮЩИЙ СПИСОК ====================
    quality_combo = GTK_COMBO_BOX_TEXT(gtk_combo_box_text_new());
    gtk_combo_box_text_append_text(quality_combo, "🌟 Лучшее (макс. качество)");
    gtk_combo_box_text_append_text(quality_combo, "🌝 1080p (Full HD)");
    gtk_combo_box_text_append_text(quality_combo, "🌗 720p (HD)");
    gtk_combo_box_text_append_text(quality_combo, "🌜 480p");
    gtk_combo_box_text_append_text(quality_combo, "🌚 360p");
    gtk_combo_box_text_append_text(quality_combo, "🌏 Только аудио (MP3)");
    gtk_combo_box_set_active(GTK_COMBO_BOX(quality_combo), 4);
    gtk_widget_set_margin_top(GTK_WIDGET(quality_combo), 3);
    gtk_widget_set_margin_bottom(GTK_WIDGET(quality_combo), 3);
    gtk_widget_set_margin_start(GTK_WIDGET(quality_combo), 15);
    gtk_widget_set_margin_end(GTK_WIDGET(quality_combo), 15);
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(quality_combo), FALSE, FALSE, 0);
    
    // ==================== ГЛАВНАЯ КНОПКА ====================
    button = gtk_button_new_with_label("📦 качка$ sudo удача 📦");
    gtk_widget_set_name(button, "download-button");
    g_signal_connect(button, "clicked", G_CALLBACK(download_clicked), NULL);
    gtk_widget_set_margin_top(button, 3);
    gtk_widget_set_margin_bottom(button, 3);
    gtk_widget_set_margin_start(button, 15);
    gtk_widget_set_margin_end(button, 15);
    gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
    
    // ==================== КНОПКА ОТКРЫТЬ ПАПКУ ====================
    button = gtk_button_new_with_label("🌀 Портал в медиатеку");
    gtk_widget_set_name(button, "open-button");
    g_signal_connect(button, "clicked", G_CALLBACK(open_folder), NULL);
    gtk_widget_set_margin_top(button, 3);
    gtk_widget_set_margin_bottom(button, 3);
    gtk_widget_set_margin_start(button, 15);
    gtk_widget_set_margin_end(button, 15);
    gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
    
    // ==================== ЛОГ ====================
    label = gtk_label_new("📋 Лог загрузок:");
    gtk_widget_set_margin_top(label, 3);
    gtk_widget_set_margin_bottom(label, 3);
    gtk_widget_set_margin_start(label, 90);
    gtk_widget_set_margin_end(label, 90);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
    
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_window), GTK_SHADOW_IN);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_margin_start(scrolled_window, 15);
    gtk_widget_set_margin_end(scrolled_window, 15);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 5);
    
    log_textview = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(log_textview), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(log_textview), FALSE);
    gtk_container_add(GTK_CONTAINER(scrolled_window), log_textview);
    
    log_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(log_textview));
    
    gtk_widget_set_size_request(scrolled_window, -1, 150);
    
    // ========== ФОНОВОЕ ИЗОБРАЖЕНИЕ И СКРУГЛЕНИЯ ==========
    GtkCssProvider *css_provider = gtk_css_provider_new();
    char css[4096];
    snprintf(css, sizeof(css),
        "window {"
        "    background-image: url('%s');"
        "    background-size: cover;"
        "    background-position: center;"
        "    background-repeat: no-repeat;"
        "}"
        "entry {"
        "    background-color: rgba(255, 255, 255, 0.7);"
        "    border-radius: 10px;"
        "    color: #784421;"
        "}"
        "#clear-button {"
        "    background-color: rgba(217, 117, 18, 0.7);"
        "    background-image: none;"
        "    border: none;"
        "    box-shadow: none;"
        "    font-weight: bold;"
        "    font-size: 18px;"
        "    border-radius: 10px;"
        "    padding: 16px 12px;"
        "    color: #000000;"
        "}"
        "#folder-button {"
        "    background-color: rgba(244, 184, 21, 0.7);"
        "    background-image: none;"
        "    border: none;"
        "    box-shadow: none;"
        "    font-weight: bold;"
        "    font-size: 18px;"
        "    border-radius: 10px;"
        "    padding: 16px 12px;"
        "    color: #000000;"
        "}"
        "#download-button {"
        "    background-color: rgba(56, 68, 94, 0.7);"
        "    background-image: none;"
        "    border: none;"
        "    box-shadow: none;"
        "    font-weight: bold;"
        "    font-size: 18px;"
        "    border-radius: 10px;"
        "    padding: 16px 12px;"
        "    color: #000000;"
        "}"
        "#open-button {"
        "    background-color: rgba(102, 44, 10, 0.7);"
        "    font-weight: bold;"
        "    font-size: 18px;"
        "    border-radius: 10px;"
        "    padding: 16px 12px;"
        "    color: white;"
        "}"
        "combobox {"
        "    background-color: rgba(27, 63, 16, 0.7);"
        "    background-image: none;"
        "    border: none;"
        "    box-shadow: none;"
        "    border-radius: 10px;"
        "    padding: 16px 12px;"
        "    font-weight: bold;"
        "    font-size: 14px;"
        "}"
        "label {"
        "    background-color: rgba(0, 0, 0, 0.7);"
        "    color: white;"
        "    padding: 2px 5px;"
        "    border-radius: 7px;"
        "}",
        background_path);
    
    gtk_css_provider_load_from_data(css_provider, css, -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css_provider);
    // =====================================================
    
    gtk_widget_show_all(window);
    
    load_config();
    
    add_to_log("🦆 Качка запущена");
    
    gtk_main();
    
    return 0;
}
