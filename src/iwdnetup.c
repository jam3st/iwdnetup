#include <syslog.h>
#include <sys/stat.h>
#include <glib.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
 
const char iwdBusName[] = "net.connman.iwd.Station";
const char iwdStateName[] = "State";
const char iwdStateTransUp[] = "connected";
const char iwdStateTransDown[] = "disconnected";
const char *iwd_down_script_path;
const char *iwd_up_script_path;


DBusHandlerResult signal_filter(DBusConnection *connection, DBusMessage *msg, void *user_data) {
    if (dbus_message_is_signal(msg, "org.freedesktop.DBus.Properties", "PropertiesChanged")) {
        DBusMessageIter iter;
        DBusMessageIter arrayIter;
        DBusMessageIter dictIter;
        char *name;

        dbus_message_iter_init(msg, &iter);
        dbus_message_iter_get_basic(&iter, &name);
        if (strncmp(iwdBusName, name, strlen(iwdBusName)) != 0)  {
            g_message("Ignoring %s", name);
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        }
        dbus_message_iter_next(&iter);
        if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY) {
            g_message("Ignoring not an array");
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        }

        dbus_message_iter_recurse(&iter, &arrayIter);
        for (dbus_bool_t hasMore = TRUE; hasMore; hasMore = dbus_message_iter_next(&arrayIter)) {
            dbus_message_iter_recurse(&arrayIter, &dictIter);
           if (dbus_message_iter_get_arg_type(&arrayIter) != DBUS_TYPE_DICT_ENTRY) {
                g_message("Ignoring not a dict");
                continue;
            }

            if (dbus_message_iter_get_arg_type(&dictIter) != DBUS_TYPE_STRING) {
                g_message("Ignoring dict not string");
                continue;
            } 

            DBusMessageIter variantIter;
            char *dictName;
            char *value;
            int variantType;

            dbus_message_iter_get_basic(&dictIter, &dictName);
            if (strncmp(iwdStateName, dictName, strlen(iwdStateName)) != 0) {
                g_message("Not expecting string %s", dictName);
                continue;
            }

            dbus_message_iter_next(&dictIter) ;
            if (dbus_message_iter_get_arg_type(&dictIter) != DBUS_TYPE_VARIANT) {
                g_message("Ignoring dict is not variant}");
            }
            dbus_message_iter_recurse(&dictIter, &variantIter);
            variantType = dbus_message_iter_get_arg_type(&variantIter);
            if(variantType != DBUS_TYPE_STRING) {
                g_message("Expected string variant");
            }
            dbus_message_iter_get_basic(&variantIter, &value);
            if (strncmp(iwdStateTransUp, value, strlen(iwdStateTransUp)) == 0) {
                syslog(LOG_LOCAL0, "IWD interface is up");
                system(iwd_up_script_path);
            } else if (strncmp(iwdStateTransDown, value, strlen(iwdStateTransDown)) == 0) {
                syslog(LOG_LOCAL0, "IWD interface is down");
                system(iwd_down_script_path);
            } else {
                g_message("Expected string variant %s", value);
            }
        }
    }
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}
 
int main(int argc, const char *argv[]) {
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    DBusError error;
    struct stat script_stat;

    if (argc != 3 || *argv[1] != '/' || *argv[2] != '/') {
        g_error("Usage %s absolue_path_non_link_for_script_up absolue_path_non_link_for_script_down", argv[0]);
        return EXIT_FAILURE;
    }
    
    iwd_up_script_path = argv[1];
    if (stat(iwd_up_script_path, &script_stat) != 0) {
        g_error("Stat says your up script file %s %s", iwd_up_script_path, strerror(errno));
        return EXIT_FAILURE;
    }

    if (!S_ISREG(script_stat.st_mode) ||
                !(script_stat.st_mode & S_IRUSR ||
                  script_stat.st_mode & S_IRGRP ||
                  script_stat.st_mode & S_IROTH) ||
                !(script_stat.st_mode & S_IXUSR ||
                  script_stat.st_mode & S_IXGRP ||
                  script_stat.st_mode & S_IXOTH)) {
        g_error("File %s is not an executable readable file for at least someone", iwd_up_script_path);
        return EXIT_FAILURE;
    }
    
    iwd_down_script_path = argv[1];
    if (stat(iwd_down_script_path, &script_stat) != 0) {
        g_error("Stat says your down script file %s %s", iwd_down_script_path, strerror(errno));
        return EXIT_FAILURE;
    }

    if (!S_ISREG(script_stat.st_mode) ||
                !(script_stat.st_mode & S_IRUSR ||
                  script_stat.st_mode & S_IRGRP ||
                  script_stat.st_mode & S_IROTH) ||
                !(script_stat.st_mode & S_IXUSR ||
                  script_stat.st_mode & S_IXGRP ||
                  script_stat.st_mode & S_IXOTH)) {
        g_error("File %s is not an executable readable file for at least someone", iwd_down_script_path);
        return EXIT_FAILURE;
    }
    
    dbus_error_init(&error);
    DBusConnection *conn = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
 
    if (dbus_error_is_set(&error)) {
        g_error("Cannot get System BUS connection: %s", error.message);
        dbus_error_free(&error);
        return EXIT_FAILURE;
    }
    dbus_connection_setup_with_g_main(conn, NULL);
 
    char *rule = "type='signal',interface='org.freedesktop.DBus.Properties',member='PropertiesChanged'";
    g_message("Signal match rule: %s", rule);
    dbus_bus_add_match(conn, rule, &error);
 
    if (dbus_error_is_set(&error)) {
        g_error("Cannot add D-BUS match rule, cause: %s", error.message);
        dbus_error_free(&error);
        return EXIT_FAILURE;
    }
 
    openlog("iwdnetup", LOG_PID|LOG_CONS, LOG_USER);
    syslog(LOG_LOCAL1,"Listening to D-BUS signals for setting up routes on iwd managed interfaces");
    dbus_connection_add_filter(conn, signal_filter, NULL, NULL);
 
    g_main_loop_run(loop);
    closelog();
 
    return EXIT_SUCCESS; 
}
