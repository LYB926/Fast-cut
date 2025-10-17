#include <gtk/gtk.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <stdlib.h>

#define APP_TITLE "Fast Cut"

typedef struct {
    GtkWidget *window;
    GtkWidget *file_entry;
    GtkWidget *start_hour_entry;
    GtkWidget *start_min_entry;
    GtkWidget *start_sec_entry;
    GtkWidget *end_hour_entry;
    GtkWidget *end_min_entry;
    GtkWidget *end_sec_entry;
    GtkWidget *preset_combo;
    GtkWidget *output_entry;
    GtkWidget *start_button;
    GtkWidget *log_view;
    GtkTextBuffer *log_buffer;
    gboolean output_customized;
    gboolean suppress_output_changed;
    gchar *output_last_auto;
    gboolean task_running;
} AppWidgets;

typedef struct {
    gint exit_status;
    gchar *stdout_buf;
    gchar *stderr_buf;
} FfmpegResult;

static void append_log_line(AppWidgets *app, const gchar *line);
static void append_log_block(AppWidgets *app, const gchar *block, const gchar *label);
static void set_output_default(AppWidgets *app, const gchar *input_path, gboolean force);
static void set_input_file(AppWidgets *app, const gchar *path);
static void on_open_file(GtkButton *button, gpointer user_data);
static void on_drag_data_received(GtkWidget *widget, GdkDragContext *context, gint x, gint y, GtkSelectionData *data, guint info, guint time, gpointer user_data);
static void on_output_changed(GtkEditable *editable, gpointer user_data);
static gboolean on_window_delete(GtkWidget *widget, GdkEvent *event, gpointer user_data);
static void on_start_clicked(GtkButton *button, gpointer user_data);
static void ffmpeg_task_thread(GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable);
static void ffmpeg_task_completed(GObject *source_object, GAsyncResult *result, gpointer user_data);
static void show_message(GtkWindow *parent, GtkMessageType type, const gchar *primary, const gchar *secondary);
static gchar *format_command_for_log(gchar **argv);
static GtkWidget *create_time_entry(const gchar *default_text);
static gboolean collect_time_string(AppWidgets *app, GtkEntry *hour_entry, GtkEntry *min_entry, GtkEntry *sec_entry, gchar **out_time, const gchar *label);
static gboolean normalize_time_entry(GtkEntry *entry, gint min_value, gint max_value, gint default_value, GtkWindow *parent, const gchar *time_label, const gchar *component_label, gint *value_out);
static gchar **build_ffmpeg_argv(const gchar *input_path, const gchar *start_time, const gchar *end_time, const gchar *preset, const gchar *output_path);
static void free_argv(gchar **argv);
static gchar *build_default_output_path(const gchar *input_path);
static void ffmpeg_result_free(FfmpegResult *result);

static const GtkTargetEntry DROP_TARGETS[] = {
    { "text/uri-list", 0, 0 }
};

int main(int argc, char **argv) {
    gtk_init(&argc, &argv);

    AppWidgets *app = g_new0(AppWidgets, 1);

    app->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app->window), APP_TITLE);
    gtk_window_set_default_size(GTK_WINDOW(app->window), 800, 600);

    GtkWidget *outer_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(outer_box), 12);
    gtk_container_add(GTK_CONTAINER(app->window), outer_box);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_box_pack_start(GTK_BOX(outer_box), grid, FALSE, FALSE, 0);

    GtkWidget *file_label = gtk_label_new("Video file:");
    gtk_widget_set_halign(file_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), file_label, 0, 0, 1, 1);

    app->file_entry = gtk_entry_new();
    gtk_editable_set_editable(GTK_EDITABLE(app->file_entry), FALSE);
    gtk_grid_attach(GTK_GRID(grid), app->file_entry, 1, 0, 1, 1);
    gtk_widget_set_hexpand(app->file_entry, TRUE);

    GtkWidget *open_button = gtk_button_new_with_label("Open file...");
    gtk_grid_attach(GTK_GRID(grid), open_button, 2, 0, 1, 1);

    GtkWidget *start_label = gtk_label_new("Start time (HH:MM:SS):");
    gtk_widget_set_halign(start_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), start_label, 0, 1, 1, 1);

    GtkWidget *start_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    app->start_hour_entry = create_time_entry("00");
    app->start_min_entry = create_time_entry("00");
    app->start_sec_entry = create_time_entry("00");
    gtk_box_pack_start(GTK_BOX(start_box), app->start_hour_entry, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(start_box), gtk_label_new(":"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(start_box), app->start_min_entry, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(start_box), gtk_label_new(":"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(start_box), app->start_sec_entry, FALSE, FALSE, 0);
    gtk_grid_attach(GTK_GRID(grid), start_box, 1, 1, 2, 1);

    GtkWidget *end_label = gtk_label_new("End time (HH:MM:SS):");
    gtk_widget_set_halign(end_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), end_label, 0, 2, 1, 1);

    GtkWidget *end_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    app->end_hour_entry = create_time_entry("00");
    app->end_min_entry = create_time_entry("00");
    app->end_sec_entry = create_time_entry("00");
    gtk_box_pack_start(GTK_BOX(end_box), app->end_hour_entry, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(end_box), gtk_label_new(":"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(end_box), app->end_min_entry, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(end_box), gtk_label_new(":"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(end_box), app->end_sec_entry, FALSE, FALSE, 0);
    gtk_grid_attach(GTK_GRID(grid), end_box, 1, 2, 2, 1);

    GtkWidget *preset_label = gtk_label_new("NVENC Preset:");
    gtk_widget_set_halign(preset_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), preset_label, 0, 3, 1, 1);

    app->preset_combo = gtk_combo_box_text_new();
    const gchar *presets[] = { "p1", "p2", "p3", "p4", "p5", "p6", "p7" };
    for (size_t i = 0; i < G_N_ELEMENTS(presets); ++i) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->preset_combo), presets[i]);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->preset_combo), 4);
    gtk_grid_attach(GTK_GRID(grid), app->preset_combo, 1, 3, 2, 1);

    GtkWidget *output_label = gtk_label_new("Output file:");
    gtk_widget_set_halign(output_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), output_label, 0, 4, 1, 1);

    app->output_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->output_entry), "Defaults to source folder");
    gtk_grid_attach(GTK_GRID(grid), app->output_entry, 1, 4, 2, 1);

    app->start_button = gtk_button_new_with_label("Start");
    gtk_widget_set_hexpand(app->start_button, FALSE);
    gtk_grid_attach(GTK_GRID(grid), app->start_button, 2, 5, 1, 1);

    GtkWidget *log_frame = gtk_frame_new("Execution log");
    gtk_box_pack_start(GTK_BOX(outer_box), log_frame, TRUE, TRUE, 0);

    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(log_frame), scrolled);

    app->log_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(app->log_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(app->log_view), FALSE);
    app->log_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app->log_view));
    gtk_container_add(GTK_CONTAINER(scrolled), app->log_view);

    gtk_drag_dest_set(app->window, GTK_DEST_DEFAULT_ALL, DROP_TARGETS, G_N_ELEMENTS(DROP_TARGETS), GDK_ACTION_COPY);
    gtk_drag_dest_set(app->file_entry, GTK_DEST_DEFAULT_ALL, DROP_TARGETS, G_N_ELEMENTS(DROP_TARGETS), GDK_ACTION_COPY);

    g_signal_connect(app->window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(app->window, "delete-event", G_CALLBACK(on_window_delete), app);
    g_signal_connect(open_button, "clicked", G_CALLBACK(on_open_file), app);
    g_signal_connect(app->window, "drag-data-received", G_CALLBACK(on_drag_data_received), app);
    g_signal_connect(app->file_entry, "drag-data-received", G_CALLBACK(on_drag_data_received), app);
    g_signal_connect(app->output_entry, "changed", G_CALLBACK(on_output_changed), app);
    g_signal_connect(app->start_button, "clicked", G_CALLBACK(on_start_clicked), app);

    gtk_widget_show_all(app->window);
    gtk_main();

    g_free(app->output_last_auto);
    g_free(app);
    return 0;
}

static void append_log_line(AppWidgets *app, const gchar *line) {
    if (!line || !*line || !app->log_buffer) {
        return;
    }
    GtkTextIter end_iter;
    gtk_text_buffer_get_end_iter(app->log_buffer, &end_iter);
    gtk_text_buffer_insert(app->log_buffer, &end_iter, line, -1);
    gtk_text_buffer_insert(app->log_buffer, &end_iter, "\n", -1);
    gtk_text_buffer_get_end_iter(app->log_buffer, &end_iter);
    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(app->log_view), &end_iter, 0.0, FALSE, 0.0, 0.0);
}

static void append_log_block(AppWidgets *app, const gchar *block, const gchar *label) {
    if (!block || !*block) {
        return;
    }
    if (label && *label) {
        append_log_line(app, label);
    }
    gchar **lines = g_strsplit(block, "\n", -1);
    if (!lines) {
        return;
    }
    for (size_t i = 0; lines[i]; ++i) {
        gchar *line = g_strchomp(lines[i]);
        if (line && *line) {
            append_log_line(app, line);
        }
    }
    g_strfreev(lines);
}

static void set_output_default(AppWidgets *app, const gchar *input_path, gboolean force) {
    if (!input_path || !*input_path) {
        return;
    }
    if (!force && app->output_customized) {
        return;
    }
    gchar *default_path = build_default_output_path(input_path);
    if (!default_path) {
        return;
    }
    app->suppress_output_changed = TRUE;
    gtk_entry_set_text(GTK_ENTRY(app->output_entry), default_path);
    app->suppress_output_changed = FALSE;
    g_free(app->output_last_auto);
    app->output_last_auto = g_strdup(default_path);
    app->output_customized = FALSE;
    g_free(default_path);
}

static void set_input_file(AppWidgets *app, const gchar *path) {
    if (!path || !*path) {
        return;
    }
    app->suppress_output_changed = TRUE;
    gtk_entry_set_text(GTK_ENTRY(app->file_entry), path);
    app->suppress_output_changed = FALSE;
    set_output_default(app, path, FALSE);
}

static void on_open_file(GtkButton *button, gpointer user_data) {
    AppWidgets *app = user_data;
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Select Video File", GTK_WINDOW(app->window), GTK_FILE_CHOOSER_ACTION_OPEN, "_Cancel", GTK_RESPONSE_CANCEL, "_Open", GTK_RESPONSE_ACCEPT, NULL);

    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Video files");
    gtk_file_filter_add_mime_type(filter, "video/*");
    gtk_file_filter_add_pattern(filter, "*.mp4");
    gtk_file_filter_add_pattern(filter, "*.mkv");
    gtk_file_filter_add_pattern(filter, "*.mov");
    gtk_file_filter_add_pattern(filter, "*.avi");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        gchar *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        set_input_file(app, filename);
        g_free(filename);
    }

    gtk_widget_destroy(dialog);
}

static void on_drag_data_received(GtkWidget *widget, GdkDragContext *context, gint x, gint y, GtkSelectionData *data, guint info, guint time, gpointer user_data) {
    AppWidgets *app = user_data;
    if (gtk_selection_data_get_length(data) <= 0) {
        gtk_drag_finish(context, FALSE, FALSE, time);
        return;
    }
    gchar **uris = g_uri_list_extract_uris((const gchar *)gtk_selection_data_get_data(data));
    if (uris && uris[0]) {
        GError *error = NULL;
        gchar *filename = g_filename_from_uri(uris[0], NULL, &error);
        if (filename) {
            set_input_file(app, filename);
            g_free(filename);
        } else if (error) {
            show_message(GTK_WINDOW(app->window), GTK_MESSAGE_ERROR, "Unable to read dropped file", error->message);
            g_clear_error(&error);
        }
    }
    g_strfreev(uris);
    gtk_drag_finish(context, TRUE, FALSE, time);
}

static void on_output_changed(GtkEditable *editable, gpointer user_data) {
    AppWidgets *app = user_data;
    if (app->suppress_output_changed) {
        return;
    }
    const gchar *text = gtk_entry_get_text(GTK_ENTRY(editable));
    if (!app->output_last_auto) {
        app->output_customized = (text && *text);
        return;
    }
    if (g_strcmp0(text, app->output_last_auto) == 0) {
        app->output_customized = FALSE;
    } else {
        app->output_customized = TRUE;
    }
}

static gboolean on_window_delete(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
    AppWidgets *app = user_data;
    if (app->task_running) {
        show_message(GTK_WINDOW(app->window), GTK_MESSAGE_INFO, "Encoding in progress", "FFmpeg is running; please wait before closing.");
        return TRUE;
    }
    return FALSE;
}

static void on_start_clicked(GtkButton *button, gpointer user_data) {
    AppWidgets *app = user_data;
    if (app->task_running) {
        return;
    }

    gchar *input_path = g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(app->file_entry))));
    gchar *start_time = NULL;
    gchar *end_time = NULL;
    gchar *output_text = g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(app->output_entry))));

    if (!input_path || !*input_path) {
        show_message(GTK_WINDOW(app->window), GTK_MESSAGE_WARNING, "Please choose a video file", NULL);
        goto cleanup_inputs;
    }
    if (!g_file_test(input_path, G_FILE_TEST_EXISTS)) {
        show_message(GTK_WINDOW(app->window), GTK_MESSAGE_ERROR, "Video file not found", input_path);
        goto cleanup_inputs;
    }
    if (!collect_time_string(app, GTK_ENTRY(app->start_hour_entry), GTK_ENTRY(app->start_min_entry), GTK_ENTRY(app->start_sec_entry), &start_time, "Start time")) {
        goto cleanup_inputs;
    }
    if (!collect_time_string(app, GTK_ENTRY(app->end_hour_entry), GTK_ENTRY(app->end_min_entry), GTK_ENTRY(app->end_sec_entry), &end_time, "End time")) {
        goto cleanup_inputs;
    }
    if (!output_text || !*output_text) {
        show_message(GTK_WINDOW(app->window), GTK_MESSAGE_WARNING, "Please enter an output file", NULL);
        goto cleanup_inputs;
    }

    gchar *preset = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(app->preset_combo));
    if (!preset || !*preset) {
        show_message(GTK_WINDOW(app->window), GTK_MESSAGE_WARNING, "Please choose an NVENC preset", NULL);
        g_free(preset);
        goto cleanup_inputs;
    }

    gchar *output_path = NULL;
    if (g_path_is_absolute(output_text)) {
        output_path = g_strdup(output_text);
    } else {
        gchar *input_dir = g_path_get_dirname(input_path);
        output_path = g_build_filename(input_dir, output_text, NULL);
        g_free(input_dir);
    }

    gchar **argv = build_ffmpeg_argv(input_path, start_time, end_time, preset, output_path);
    if (!argv) {
        show_message(GTK_WINDOW(app->window), GTK_MESSAGE_ERROR, "Unable to build ffmpeg command", NULL);
        g_free(preset);
        g_free(output_path);
        goto cleanup_inputs;
    }

    gchar *log_line = g_strdup_printf("Input: %s", input_path);
    append_log_line(app, "Starting ffmpeg...");
    append_log_line(app, log_line);
    g_free(log_line);

    log_line = g_strdup_printf("Output: %s", output_path);
    append_log_line(app, log_line);
    g_free(log_line);

    gchar *command_line = format_command_for_log(argv);
    if (command_line) {
        append_log_line(app, "Command:");
        append_log_line(app, command_line);
        g_free(command_line);
    }

    app->task_running = TRUE;
    gtk_widget_set_sensitive(app->start_button, FALSE);

    GTask *task = g_task_new(app->window, NULL, ffmpeg_task_completed, app);
    g_task_set_task_data(task, argv, (GDestroyNotify)free_argv);
    g_task_run_in_thread(task, ffmpeg_task_thread);
    g_object_unref(task);

    g_free(preset);
    g_free(output_path);
    goto cleanup_inputs;

cleanup_inputs:
    g_free(input_path);
    g_free(start_time);
    g_free(end_time);
    g_free(output_text);
}

static void ffmpeg_task_thread(GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable) {
    gchar **argv = task_data;
    if (!argv) {
        g_task_return_new_error(task, G_SPAWN_ERROR, G_SPAWN_ERROR_FAILED, "Argument list is empty");
        return;
    }

    gchar *stdout_buf = NULL;
    gchar *stderr_buf = NULL;
    gint exit_status = 0;
    GError *error = NULL;

    gboolean ok = g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, &stdout_buf, &stderr_buf, &exit_status, &error);
    if (!ok) {
        g_task_return_error(task, error);
        g_free(stdout_buf);
        g_free(stderr_buf);
        return;
    }

    FfmpegResult *result = g_new0(FfmpegResult, 1);
    result->exit_status = exit_status;
    result->stdout_buf = stdout_buf;
    result->stderr_buf = stderr_buf;
    g_task_return_pointer(task, result, (GDestroyNotify)ffmpeg_result_free);
}

static void ffmpeg_task_completed(GObject *source_object, GAsyncResult *result, gpointer user_data) {
    AppWidgets *app = user_data;
    app->task_running = FALSE;
    gtk_widget_set_sensitive(app->start_button, TRUE);

    GError *error = NULL;
    FfmpegResult *ff_result = g_task_propagate_pointer(G_TASK(result), &error);
    if (error) {
        append_log_line(app, "ffmpeg failed:");
        append_log_line(app, error->message);
        show_message(GTK_WINDOW(app->window), GTK_MESSAGE_ERROR, "ffmpeg failed", error->message);
        g_error_free(error);
        return;
    }

    if (ff_result->stdout_buf && *ff_result->stdout_buf) {
        append_log_block(app, ff_result->stdout_buf, "ffmpeg stdout:");
    }
    if (ff_result->stderr_buf && *ff_result->stderr_buf) {
        append_log_block(app, ff_result->stderr_buf, "ffmpeg stderr:");
    }

    if (ff_result->exit_status == 0) {
        append_log_line(app, "ffmpeg completed with exit code 0.");
        show_message(GTK_WINDOW(app->window), GTK_MESSAGE_INFO, "Done", "FFmpeg finished successfully.");
    } else {
        gchar *status_msg = g_strdup_printf("ffmpeg exit code %d", ff_result->exit_status);
        append_log_line(app, status_msg);
        show_message(GTK_WINDOW(app->window), GTK_MESSAGE_WARNING, "ffmpeg did not finish successfully", status_msg);
        g_free(status_msg);
    }

    ffmpeg_result_free(ff_result);
}

static void show_message(GtkWindow *parent, GtkMessageType type, const gchar *primary, const gchar *secondary) {
    GtkWidget *dialog = gtk_message_dialog_new(parent, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, type, GTK_BUTTONS_OK, "%s", primary ? primary : "");
    if (secondary && *secondary) {
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s", secondary);
    }
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

static gchar *format_command_for_log(gchar **argv) {
    if (!argv) {
        return NULL;
    }
    GPtrArray *parts = g_ptr_array_new_with_free_func(g_free);
    for (guint i = 0; argv[i]; ++i) {
        gchar *quoted = g_shell_quote(argv[i]);
        if (quoted) {
            g_ptr_array_add(parts, quoted);
        }
    }
    g_ptr_array_add(parts, NULL);
    gchar *joined = g_strjoinv(" ", (gchar **)parts->pdata);
    g_ptr_array_free(parts, TRUE);
    return joined;
}

static GtkWidget *create_time_entry(const gchar *default_text) {
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(entry), 2);
    gtk_entry_set_max_length(GTK_ENTRY(entry), 2);
    gtk_entry_set_alignment(GTK_ENTRY(entry), 0.5f);
    gtk_entry_set_input_purpose(GTK_ENTRY(entry), GTK_INPUT_PURPOSE_DIGITS);
    if (default_text) {
        gtk_entry_set_text(GTK_ENTRY(entry), default_text);
    }
    return entry;
}

static gboolean normalize_time_entry(GtkEntry *entry, gint min_value, gint max_value, gint default_value, GtkWindow *parent, const gchar *time_label, const gchar *component_label, gint *value_out) {
    gchar *raw = g_strstrip(g_strdup(gtk_entry_get_text(entry)));
    if (!raw || !*raw) {
        *value_out = default_value;
    } else {
        for (gchar *p = raw; *p; ++p) {
            if (!g_ascii_isdigit(*p)) {
                gchar *message = g_strdup_printf("%s %s accepts digits only.", time_label, component_label);
                show_message(parent, GTK_MESSAGE_WARNING, message, NULL);
                g_free(message);
                g_free(raw);
                return FALSE;
            }
        }
        long parsed = strtol(raw, NULL, 10);
        if (parsed < min_value || parsed > max_value) {
            gchar *message = g_strdup_printf("%s %s must be between %02d and %02d.", time_label, component_label, min_value, max_value);
            show_message(parent, GTK_MESSAGE_WARNING, message, NULL);
            g_free(message);
            g_free(raw);
            return FALSE;
        }
        *value_out = (gint)parsed;
    }
    gchar *normalized = g_strdup_printf("%02d", *value_out);
    gtk_entry_set_text(entry, normalized);
    g_free(normalized);
    g_free(raw);
    return TRUE;
}

static gboolean collect_time_string(AppWidgets *app, GtkEntry *hour_entry, GtkEntry *min_entry, GtkEntry *sec_entry, gchar **out_time, const gchar *label) {
    gint hour = 0;
    gint minute = 0;
    gint second = 0;
    g_free(*out_time);
    *out_time = NULL;
    if (!normalize_time_entry(hour_entry, 0, 99, 0, GTK_WINDOW(app->window), label, "hour", &hour)) {
        return FALSE;
    }
    if (!normalize_time_entry(min_entry, 0, 59, 0, GTK_WINDOW(app->window), label, "minute", &minute)) {
        return FALSE;
    }
    if (!normalize_time_entry(sec_entry, 0, 59, 0, GTK_WINDOW(app->window), label, "second", &second)) {
        return FALSE;
    }
    g_free(*out_time);
    *out_time = g_strdup_printf("%02d:%02d:%02d", hour, minute, second);
    return TRUE;
}

static gchar **build_ffmpeg_argv(const gchar *input_path, const gchar *start_time, const gchar *end_time, const gchar *preset, const gchar *output_path) {
    if (!input_path || !start_time || !end_time || !preset || !output_path) {
        return NULL;
    }
    gchar **argv = g_new0(gchar *, 13 + 1);
    guint idx = 0;
    //argv[idx++] = g_strdup("D:\\Software\\ffmpeg\\bin\\ffmpeg.exe");
    argv[idx++] = g_strdup("ffmpeg");
    argv[idx++] = g_strdup("-y");
    argv[idx++] = g_strdup("-ss");
    argv[idx++] = g_strdup(start_time);
    argv[idx++] = g_strdup("-to");
    argv[idx++] = g_strdup(end_time);
    argv[idx++] = g_strdup("-i");
    argv[idx++] = g_strdup(input_path);
    argv[idx++] = g_strdup("-c:v");
    argv[idx++] = g_strdup("hevc_nvenc");
    //argv[idx++] = g_strdup("hevc_qsv");
    argv[idx++] = g_strdup("-preset");
    argv[idx++] = g_strdup(preset);
    //argv[idx++] = g_strdup("veryslow");
    argv[idx++] = g_strdup(output_path);
    argv[idx] = NULL;
    return argv;
}

static void free_argv(gchar **argv) {
    if (!argv) {
        return;
    }
    for (guint i = 0; argv[i]; ++i) {
        g_free(argv[i]);
    }
    g_free(argv);
}

static void ffmpeg_result_free(FfmpegResult *result) {
    if (!result) {
        return;
    }
    g_free(result->stdout_buf);
    g_free(result->stderr_buf);
    g_free(result);
}

static gchar *build_default_output_path(const gchar *input_path) {
    if (!input_path || !*input_path) {
        return NULL;
    }
    gchar *dir = g_path_get_dirname(input_path);
    gchar *base = g_path_get_basename(input_path);
    gchar *dot = g_strrstr(base, ".");
    if (dot && dot != base) {
        *dot = '\0';
    }
    gchar *new_name = g_strdup_printf("%s_hevc.mp4", base);
    gchar *full_path = g_build_filename(dir, new_name, NULL);
    g_free(dir);
    g_free(base);
    g_free(new_name);
    return full_path;
}
