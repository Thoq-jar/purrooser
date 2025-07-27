#include <adwaita.h>
#include <gtk/gtk.h>
#include <webkitgtk-6.0/webkit/webkit.h>

typedef struct {
  GtkWidget* webview;
  GtkWidget* url_entry;
  char* title;
  char* url;
} Tab;

typedef struct {
  GtkWidget* window;
  GtkWidget* headerbar;
  AdwTabView* tab_view;
  GtkWidget* back_button;
  GtkWidget* forward_button;
  GtkWidget* reload_button;
  GtkWidget* add_tab_button;
  GtkWidget* menu_button;
  GtkWidget* settings_window;
  GtkWidget* homepage_entry;
  GtkWidget* js_switch;
  GtkWidget* webgl_switch;
  GtkWidget* media_switch;
  char* homepage;
  gboolean enable_js;
  gboolean enable_webgl;
  gboolean enable_media;
  gboolean auto_switch_to_new_tab;
} WebViewApp;

static void update_navigation_buttons(WebViewApp* app);
static void on_load_changed(WebKitWebView* webview, WebKitLoadEvent load_event, WebViewApp* app);
static void on_url_activate(GtkEntry* entry, WebViewApp* app);
static void on_back_clicked(GtkButton* button, WebViewApp* app);
static void on_forward_clicked(GtkButton* button, WebViewApp* app);
static void on_reload_clicked(GtkButton* button, WebViewApp* app);
static void free_tab(Tab* tab);
static void apply_settings_to_webview(WebKitWebView* webview, WebViewApp* app);
static Tab* create_new_tab(WebViewApp* app, const char* url);
static void on_add_tab_clicked(GtkButton* button, WebViewApp* app);
static void on_tab_selected(AdwTabView* tab_view, AdwTabPage* page, WebViewApp* app);
static gboolean on_decide_policy(WebKitWebView* webview, WebKitPolicyDecision* decision, 
                                WebKitPolicyDecisionType type, WebViewApp* app);
static void on_create_web_view(WebKitWebView* webview, WebKitNavigationAction* action, WebViewApp* app);
static void on_settings_save(GtkButton* button, WebViewApp* app);
static void show_settings(GtkButton* button, WebViewApp* app);
static void activate(GtkApplication* app, gpointer user_data);

static void update_navigation_buttons(WebViewApp* app) {
  AdwTabPage* page = adw_tab_view_get_selected_page(app->tab_view);
  if (!page) return;
  
  Tab* tab = g_object_get_data(G_OBJECT(page), "tab-data");
  if (!tab) return;
  
  WebKitWebView* webview = WEBKIT_WEB_VIEW(tab->webview);
  gtk_widget_set_sensitive(app->back_button, webkit_web_view_can_go_back(webview));
  gtk_widget_set_sensitive(app->forward_button, webkit_web_view_can_go_forward(webview));
}

static void on_load_changed(WebKitWebView* webview, WebKitLoadEvent load_event, WebViewApp* app) {
  g_print("Load event: %d\n", load_event);

  AdwTabPage* page = adw_tab_view_get_selected_page(app->tab_view);
  if (!page) return;

  Tab* tab = g_object_get_data(G_OBJECT(page), "tab-data");
  if (!tab || tab->webview != GTK_WIDGET(webview)) return;

  switch (load_event) {
    case WEBKIT_LOAD_STARTED:
      gtk_widget_set_sensitive(app->reload_button, FALSE);
      break;
    case WEBKIT_LOAD_COMMITTED:
      break;
    case WEBKIT_LOAD_FINISHED:
      gtk_widget_set_sensitive(app->reload_button, TRUE);
      const char* uri = webkit_web_view_get_uri(webview);
      if (uri) {
        gtk_editable_set_text(GTK_EDITABLE(tab->url_entry), uri);
        g_free(tab->url);
        tab->url = g_strdup(uri);
      }

      const char* title = webkit_web_view_get_title(webview);
      if (title) {
        g_free(tab->title);
        tab->title = g_strdup(title);
        adw_tab_page_set_title(page, title);
      }
      break;
    case WEBKIT_LOAD_REDIRECTED:
      break;
  }

  update_navigation_buttons(app);
}

static void on_url_activate(GtkEntry* entry, WebViewApp* app) {
  const char* url = gtk_editable_get_text(GTK_EDITABLE(entry));

  char* full_url;
  if (strstr(url, "://") == NULL) {
    full_url = g_strdup_printf("https://%s", url);
  } else {
    full_url = g_strdup(url);
  }

  AdwTabPage* page = adw_tab_view_get_selected_page(app->tab_view);
  if (page) {
    Tab* tab = g_object_get_data(G_OBJECT(page), "tab-data");
    if (tab) {
      webkit_web_view_load_uri(WEBKIT_WEB_VIEW(tab->webview), full_url);
    }
  }

  g_free(full_url);
}

static void on_back_clicked(GtkButton* button, WebViewApp* app) {
  AdwTabPage* page = adw_tab_view_get_selected_page(app->tab_view);
  if (page) {
    Tab* tab = g_object_get_data(G_OBJECT(page), "tab-data");
    if (tab) {
      webkit_web_view_go_back(WEBKIT_WEB_VIEW(tab->webview));
    }
  }
}

static void on_forward_clicked(GtkButton* button, WebViewApp* app) {
  AdwTabPage* page = adw_tab_view_get_selected_page(app->tab_view);
  if (page) {
    Tab* tab = g_object_get_data(G_OBJECT(page), "tab-data");
    if (tab) {
      webkit_web_view_go_forward(WEBKIT_WEB_VIEW(tab->webview));
    }
  }
}

static void on_reload_clicked(GtkButton* button, WebViewApp* app) {
  AdwTabPage* page = adw_tab_view_get_selected_page(app->tab_view);
  if (page) {
    Tab* tab = g_object_get_data(G_OBJECT(page), "tab-data");
    if (tab) {
      webkit_web_view_reload(WEBKIT_WEB_VIEW(tab->webview));
    }
  }
}

static void free_tab(Tab* tab) {
  g_free(tab->title);
  g_free(tab->url);
  g_free(tab);
}

static void apply_settings_to_webview(WebKitWebView* webview, WebViewApp* app) {
  WebKitSettings* settings = webkit_web_view_get_settings(webview);
  webkit_settings_set_enable_javascript(settings, app->enable_js);
  webkit_settings_set_enable_webgl(settings, app->enable_webgl);
  webkit_settings_set_enable_media_stream(settings, app->enable_media);
  webkit_settings_set_hardware_acceleration_policy(settings, WEBKIT_HARDWARE_ACCELERATION_POLICY_ALWAYS);
  g_object_set(G_OBJECT(settings), "enable-developer-extras", TRUE, NULL);
}

static Tab* create_new_tab(WebViewApp* app, const char* url) {
  g_print("Creating new tab with URL: %s\n", url ? url : "NULL");
  
  Tab* tab = g_new0(Tab, 1);
  
  tab->webview = webkit_web_view_new();
  if (!tab->webview) {
    g_print("Failed to create webview\n");
    g_free(tab);
    return NULL;
  }
  
  apply_settings_to_webview(WEBKIT_WEB_VIEW(tab->webview), app);
  
  tab->url_entry = gtk_entry_new();
  gtk_entry_set_placeholder_text(GTK_ENTRY(tab->url_entry), "Enter URL or search...");
  gtk_widget_set_hexpand(tab->url_entry, TRUE);
  gtk_widget_add_css_class(tab->url_entry, "pill");
  
  tab->title = g_strdup("New Tab");
  tab->url = g_strdup(url ? url : app->homepage);
  
  GtkWidget* scrolled = gtk_scrolled_window_new();
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), tab->webview);
  
  AdwTabPage* page = adw_tab_view_add_page(app->tab_view, scrolled, NULL);
  if (!page) {
    g_print("Failed to add page to tab view\n");
    free_tab(tab);
    return NULL;
  }
  
  adw_tab_page_set_title(page, tab->title);
  g_print("Added page with title: %s\n", tab->title);
  
  g_object_set_data_full(G_OBJECT(page), "tab-data", tab, (GDestroyNotify)free_tab);
  
  g_signal_connect(tab->webview, "load-changed", G_CALLBACK(on_load_changed), app);
  g_signal_connect(tab->webview, "decide-policy", G_CALLBACK(on_decide_policy), app);
  g_signal_connect(tab->webview, "create", G_CALLBACK(on_create_web_view), app);
  g_signal_connect(tab->url_entry, "activate", G_CALLBACK(on_url_activate), app);
  
  if (tab->url) {
    g_print("Loading URL: %s\n", tab->url);
    webkit_web_view_load_uri(WEBKIT_WEB_VIEW(tab->webview), tab->url);
  }
  
  return tab;
}

static void on_add_tab_clicked(GtkButton* button, WebViewApp* app) {
  g_print("Add tab button clicked\n");
  
  if (!app || !app->tab_view) {
    g_print("App or tab_view is NULL\n");
    return;
  }
  
  Tab* new_tab = create_new_tab(app, NULL);
  if (!new_tab) {
    g_print("Failed to create new tab\n");
    return;
  }

  int n_pages = adw_tab_view_get_n_pages(app->tab_view);
  
  if (app->auto_switch_to_new_tab && n_pages > 0) {
    AdwTabPage* new_page = adw_tab_view_get_nth_page(app->tab_view, n_pages - 1);
    adw_tab_view_set_selected_page(app->tab_view, new_page);
    g_print("Selected new tab\n");
  }
}

static void on_tab_selected(AdwTabView* tab_view, AdwTabPage* page, WebViewApp* app) {
  g_print("Tab selected callback called\n");
  if (!page) {
    g_print("No page selected\n");
    return;
  }
  
  Tab* tab = g_object_get_data(G_OBJECT(page), "tab-data");
  if (!tab) {
    g_print("No tab data found\n");
    return;
  }
  
  g_print("Switching to tab: %s\n", tab->title);
  adw_header_bar_set_title_widget(ADW_HEADER_BAR(app->headerbar), tab->url_entry);
  update_navigation_buttons(app);
}

static gboolean on_decide_policy(WebKitWebView* webview, WebKitPolicyDecision* decision, 
                                WebKitPolicyDecisionType type, WebViewApp* app) {
    if (type == WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION) {
        WebKitNavigationPolicyDecision* nav_decision = WEBKIT_NAVIGATION_POLICY_DECISION(decision);
        WebKitNavigationAction* action = webkit_navigation_policy_decision_get_navigation_action(nav_decision);
        guint button = webkit_navigation_action_get_mouse_button(action);
        guint modifiers = webkit_navigation_action_get_modifiers(action);

        if (button == 2 || (button == 1 && (modifiers & GDK_CONTROL_MASK))) {
            WebKitURIRequest* request = webkit_navigation_action_get_request(action);
            const char* uri = webkit_uri_request_get_uri(request);
            
            Tab* new_tab = create_new_tab(app, uri);
            if (new_tab) {
                int n_pages = adw_tab_view_get_n_pages(app->tab_view);
                if (n_pages > 0) {
                    AdwTabPage* new_page = adw_tab_view_get_nth_page(app->tab_view, n_pages - 1);
                    adw_tab_view_set_selected_page(app->tab_view, new_page);
                    g_print("Switched to new tab\n");
                }
            }
            webkit_policy_decision_ignore(decision);
            return TRUE;
        }
    }
    return FALSE;
}

static void on_create_web_view(WebKitWebView* webview, WebKitNavigationAction* action, WebViewApp* app) {
  WebKitURIRequest* request = webkit_navigation_action_get_request(action);
  const char* uri = webkit_uri_request_get_uri(request);

  Tab* new_tab = create_new_tab(app, uri);
  if (new_tab) {
    int n_pages = adw_tab_view_get_n_pages(app->tab_view);
    if (n_pages > 0) {
      AdwTabPage* page = adw_tab_view_get_nth_page(app->tab_view, n_pages - 1);
      if (page) {
        adw_tab_view_set_selected_page(app->tab_view, page);
        g_print("Switched to new tab\n");
      }
    }
  }
}

static void on_settings_save(GtkButton* button, WebViewApp* app) {
  const char* homepage = gtk_editable_get_text(GTK_EDITABLE(app->homepage_entry));
  g_free(app->homepage);
  app->homepage = g_strdup(homepage);
  
  app->enable_js = gtk_switch_get_active(GTK_SWITCH(app->js_switch));
  app->enable_webgl = gtk_switch_get_active(GTK_SWITCH(app->webgl_switch));
  app->enable_media = gtk_switch_get_active(GTK_SWITCH(app->media_switch));
  
  int n_pages = adw_tab_view_get_n_pages(app->tab_view);
  
  for (int i = 0; i < n_pages; i++) {
    AdwTabPage* page = adw_tab_view_get_nth_page(app->tab_view, i);
    Tab* tab = g_object_get_data(G_OBJECT(page), "tab-data");
    if (tab) {
      apply_settings_to_webview(WEBKIT_WEB_VIEW(tab->webview), app);
    }
  }
  
  gtk_window_close(GTK_WINDOW(app->settings_window));
}

static void show_settings(GtkButton* button, WebViewApp* app) {
  if (!app->settings_window) {
    app->settings_window = adw_window_new();
    gtk_window_set_title(GTK_WINDOW(app->settings_window), "Settings");
    gtk_window_set_default_size(GTK_WINDOW(app->settings_window), 400, 300);
    gtk_window_set_transient_for(GTK_WINDOW(app->settings_window), GTK_WINDOW(app->window));
    gtk_window_set_modal(GTK_WINDOW(app->settings_window), TRUE);
    
    GtkWidget* headerbar = adw_header_bar_new();
    
    GtkWidget* content_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(content_box, 24);
    gtk_widget_set_margin_bottom(content_box, 24);
    gtk_widget_set_margin_start(content_box, 24);
    gtk_widget_set_margin_end(content_box, 24);
    
    GtkWidget* homepage_label = gtk_label_new("Homepage:");
    gtk_widget_set_halign(homepage_label, GTK_ALIGN_START);
    app->homepage_entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(app->homepage_entry), app->homepage);
    
    GtkWidget* js_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget* js_label = gtk_label_new("Enable JavaScript");
    app->js_switch = gtk_switch_new();
    gtk_switch_set_active(GTK_SWITCH(app->js_switch), app->enable_js);
    gtk_widget_set_hexpand(js_label, TRUE);
    gtk_widget_set_halign(js_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(js_box), js_label);
    gtk_box_append(GTK_BOX(js_box), app->js_switch);
    
    GtkWidget* webgl_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget* webgl_label = gtk_label_new("Enable WebGL");
    app->webgl_switch = gtk_switch_new();
    gtk_switch_set_active(GTK_SWITCH(app->webgl_switch), app->enable_webgl);
    gtk_widget_set_hexpand(webgl_label, TRUE);
    gtk_widget_set_halign(webgl_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(webgl_box), webgl_label);
    gtk_box_append(GTK_BOX(webgl_box), app->webgl_switch);
    
    GtkWidget* media_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget* media_label = gtk_label_new("Enable Media Stream");
    app->media_switch = gtk_switch_new();
    gtk_switch_set_active(GTK_SWITCH(app->media_switch), app->enable_media);
    gtk_widget_set_hexpand(media_label, TRUE);
    gtk_widget_set_halign(media_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(media_box), media_label);
    gtk_box_append(GTK_BOX(media_box), app->media_switch);
    
    GtkWidget* save_button = gtk_button_new_with_label("Save");
    gtk_widget_add_css_class(save_button, "suggested-action");
    gtk_widget_set_halign(save_button, GTK_ALIGN_END);
    
    gtk_box_append(GTK_BOX(content_box), homepage_label);
    gtk_box_append(GTK_BOX(content_box), app->homepage_entry);
    gtk_box_append(GTK_BOX(content_box), js_box);
    gtk_box_append(GTK_BOX(content_box), webgl_box);
    gtk_box_append(GTK_BOX(content_box), media_box);
    gtk_box_append(GTK_BOX(content_box), save_button);
    
    GtkWidget* main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(main_box), headerbar);
    gtk_box_append(GTK_BOX(main_box), content_box);
    
    adw_window_set_content(ADW_WINDOW(app->settings_window), main_box);
    
    g_signal_connect(save_button, "clicked", G_CALLBACK(on_settings_save), app);
  }
  
  gtk_window_present(GTK_WINDOW(app->settings_window));
}

static void activate(GtkApplication* app, gpointer user_data) {
  WebViewApp* webview_app = g_new0(WebViewApp, 1);
  
  webview_app->homepage = g_strdup("https://startpage.com");
  webview_app->enable_js = TRUE;
  webview_app->enable_webgl = TRUE;
  webview_app->enable_media = TRUE;
  webview_app->auto_switch_to_new_tab = TRUE;

  webview_app->window = adw_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(webview_app->window), "Purrooser");
  gtk_window_set_default_size(GTK_WINDOW(webview_app->window), 1200, 800);

  webview_app->headerbar = adw_header_bar_new();

  webview_app->back_button = gtk_button_new_from_icon_name("go-previous-symbolic");
  webview_app->forward_button = gtk_button_new_from_icon_name("go-next-symbolic");
  webview_app->reload_button = gtk_button_new_from_icon_name("view-refresh-symbolic");
  webview_app->add_tab_button = gtk_button_new_from_icon_name("tab-new-symbolic");

  gtk_widget_set_sensitive(webview_app->back_button, FALSE);
  gtk_widget_set_sensitive(webview_app->forward_button, FALSE);

  gtk_widget_add_css_class(webview_app->back_button, "flat");
  gtk_widget_add_css_class(webview_app->forward_button, "flat");
  gtk_widget_add_css_class(webview_app->reload_button, "flat");
  gtk_widget_add_css_class(webview_app->add_tab_button, "flat");

  webview_app->menu_button = gtk_button_new_from_icon_name("open-menu-symbolic");
  gtk_widget_add_css_class(webview_app->menu_button, "flat");

  GtkWidget* nav_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_append(GTK_BOX(nav_box), webview_app->back_button);
  gtk_box_append(GTK_BOX(nav_box), webview_app->forward_button);
  gtk_box_append(GTK_BOX(nav_box), webview_app->reload_button);
  gtk_box_append(GTK_BOX(nav_box), webview_app->add_tab_button);

  adw_header_bar_pack_start(ADW_HEADER_BAR(webview_app->headerbar), nav_box);
  adw_header_bar_pack_end(ADW_HEADER_BAR(webview_app->headerbar), webview_app->menu_button);

  webview_app->tab_view = adw_tab_view_new();
  adw_tab_view_set_menu_model(webview_app->tab_view, NULL);

  AdwTabBar* tab_bar = adw_tab_bar_new();
  adw_tab_bar_set_view(tab_bar, webview_app->tab_view);
  adw_tab_bar_set_autohide(tab_bar, FALSE);

  GtkWidget* main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_append(GTK_BOX(main_box), webview_app->headerbar);
  gtk_box_append(GTK_BOX(main_box), GTK_WIDGET(tab_bar));
  gtk_box_append(GTK_BOX(main_box), GTK_WIDGET(webview_app->tab_view));
  gtk_widget_set_vexpand(GTK_WIDGET(webview_app->tab_view), TRUE);

  adw_application_window_set_content(ADW_APPLICATION_WINDOW(webview_app->window), main_box);

  g_signal_connect(webview_app->add_tab_button, "clicked", G_CALLBACK(on_add_tab_clicked), webview_app);
  g_signal_connect(webview_app->back_button, "clicked", G_CALLBACK(on_back_clicked), webview_app);
  g_signal_connect(webview_app->forward_button, "clicked", G_CALLBACK(on_forward_clicked), webview_app);
  g_signal_connect(webview_app->reload_button, "clicked", G_CALLBACK(on_reload_clicked), webview_app);
  g_signal_connect(webview_app->menu_button, "clicked", G_CALLBACK(show_settings), webview_app);
  g_signal_connect(webview_app->tab_view, "page-attached", G_CALLBACK(on_tab_selected), webview_app);

  g_print("Creating first tab...\n");
  Tab* first_tab = create_new_tab(webview_app, NULL);
  if (first_tab) {
    g_print("First tab created successfully\n");
    AdwTabPage* first_page = adw_tab_view_get_nth_page(webview_app->tab_view, 0);
    if (first_page) {
      adw_tab_view_set_selected_page(webview_app->tab_view, first_page);
      adw_header_bar_set_title_widget(ADW_HEADER_BAR(webview_app->headerbar), first_tab->url_entry);
    }
  } else {
    g_print("Failed to create first tab\n");
  }

  gtk_window_present(GTK_WINDOW(webview_app->window));

  g_object_set_data_full(G_OBJECT(webview_app->window), "app-data", webview_app, g_free);
}

int main(int argc, char** argv) {
  AdwApplication* app;
  int status;

  app = adw_application_new("dev.thoq.purrooser", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

  status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);

  return status;
}