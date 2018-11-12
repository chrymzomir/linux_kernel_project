#include <gtk/gtk.h>

static void on_button_clicked(GtkButton *button,gpointer data);
char *path;
static void initialize_window(GtkWidget* window)
{
  gtk_window_set_title(GTK_WINDOW(window),"KeyLogger");
  gtk_window_set_default_size (GTK_WINDOW (window), 300, 70); 
  gtk_window_set_position(GTK_WINDOW(window),GTK_WIN_POS_CENTER);
  gtk_container_set_border_width(GTK_CONTAINER(window),5);
}

int main(int argc, char *argv[])
{
    GtkWidget *window;
    GtkWidget *entry;
    GtkWidget *button;
    GtkWidget *hbox;
    GtkWidget *label;

    gtk_init(&argc,&argv);
    window=gtk_window_new(GTK_WINDOW_TOPLEVEL);
    initialize_window(window);
    
    hbox=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,5);
    gtk_container_add(GTK_CONTAINER(window),hbox);
    label = gtk_label_new("Please enter an output file");
    gtk_box_pack_start(GTK_BOX(hbox),label,FALSE,FALSE,0);

    entry=gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox),entry,TRUE,TRUE,5);

    button=gtk_button_new_with_label("submit");
    gtk_box_pack_start(GTK_BOX(hbox),button,TRUE,TRUE,0);

    g_signal_connect(G_OBJECT(window),"destroy",
                G_CALLBACK(gtk_main_quit),NULL);
    g_signal_connect(G_OBJECT(button),"clicked",
                G_CALLBACK(on_button_clicked),entry);

    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}

static void on_button_clicked(GtkButton *button,gpointer data)
{
    GtkWidget *entry=(GtkWidget *)data;
    GtkWidget *dialog;
    gchar buff[500];

    g_snprintf(buff,500,"Your file is '%s' !",
                gtk_entry_get_text(GTK_ENTRY(entry)));
    path =  gtk_entry_get_text(GTK_ENTRY(entry));
    dialog=gtk_message_dialog_new(NULL,GTK_DIALOG_MODAL,
                GTK_MESSAGE_INFO,GTK_BUTTONS_OK,"%s",buff);
    gtk_dialog_run(GTK_DIALOG(dialog));

    gtk_widget_destroy(dialog);
    gtk_entry_set_text(GTK_ENTRY(entry),"");
    system("keylogger.c");
}
