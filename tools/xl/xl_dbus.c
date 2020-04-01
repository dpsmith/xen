#include <glib.h>
#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <strings.h>

#include <libxl.h>

#include "xl.h"
#include "xl_dbus.h"

const char *dbus_service_name = "org.xen.xl";
//const char *dbus_argv_intf = "org.xen.xl.Argv";
const char *dbus_root_obj = "/org/xen/xl";

struct xl_dbus_service {
    GDBusNodeInfo *introspection_data;

    guint owner_id;
    guint registration_id;

    GDBusInterfaceVTable interface_vtable;

    GMainLoop *loop;
};

static void free_service(gpointer service) {
    struct xl_dbus_service *svc = (struct xl_dbus_service *)service;

    g_dbus_node_info_unref(svc->introspection_data);
    g_free(svc);
}

static void handle_method_call(GDBusConnection       *connection,
                               const gchar           *sender,
                               const gchar           *object_path,
                               const gchar           *interface_name,
                               const gchar           *method_name,
                               GVariant              *parameters,
                               GDBusMethodInvocation *invocation,
                               gpointer               user_data)
{
    const gchar *args;
    gchar **argv;
    gint argc;
    int pipefds[2];
    int ret;
    struct cmd_spec *cspec;

    if (!(g_dbus_connection_get_capabilities(connection) &
        G_DBUS_CAPABILITY_FLAGS_UNIX_FD_PASSING)) {
          g_dbus_method_invocation_return_dbus_error(invocation,
            "org.xen.xl.Error.Failed",
            "Your message bus daemon does not support file descriptor passing (need D-Bus >= 1.3.0)");
    }

    /* fetch argv string and split for passing */
    g_variant_get(parameters, "(&s)", &args);
    if (strlen(args) > 0) {
        if (!g_shell_parse_argv(args, &argc, &argv, NULL)) {
            g_dbus_method_invocation_return_dbus_error(invocation,
                "org.xen.xl.Error.Failed", "Error occurred processing parameters");
            goto out;
        }
    } else {
	/* fake argv[0] */
        argc = 1;
        argv = (gchar **)g_malloc(sizeof(gchar *));
	*argv = (gchar *)g_malloc(sizeof("xl-dbus\0"));
	memcpy(*argv, "xl-dbus\0", sizeof("xl-dbus\0"));
    }

    if (pipe(pipefds) < 0) {
        g_dbus_method_invocation_return_dbus_error(invocation,
            "org.xen.xl.Error.Failed", "Error occurred creating file descriptor");
        goto clean_argv;
    }

    if (dup2(pipefds[0], STDOUT_FILENO) < 0) {
        g_dbus_method_invocation_return_dbus_error(invocation,
            "org.xen.xl.Error.Failed", "Error occurred attaching to STDOUT");
        goto clean_pipe;
    }

    cspec = cmdtable_lookup((char *)method_name);
    if (cspec) {
        if (dryrun_only && !cspec->can_dryrun) {
            fprintf(stderr, "command does not implement -N (dryrun) option\n");
            g_dbus_method_invocation_return_dbus_error(invocation,
                "org.xen.xl.Error.NotImplemented",
                "Requested method does not implement dryrun");
            goto clean_pipe;
        }

        ret = cspec->cmd_impl((int)argc, (char **)argv);
        if (ret == 0) {
            GDBusMessage *reply;
            GUnixFDList *fd_list;
            GError *error;

            fd_list = g_unix_fd_list_new();
            error = NULL;
            g_unix_fd_list_append(fd_list, pipefds[1], &error);
            g_assert_no_error(error);

            reply = g_dbus_message_new_method_reply(
                g_dbus_method_invocation_get_message(invocation));
            g_dbus_message_set_unix_fd_list(reply, fd_list);

            error = NULL;
            g_dbus_connection_send_message(connection, reply,
                G_DBUS_SEND_MESSAGE_FLAGS_NONE, NULL, &error);
            g_assert_no_error (error);

            g_object_unref(invocation);
            g_object_unref(fd_list);
            g_object_unref(reply);
        } else {
            g_dbus_method_invocation_return_dbus_error (invocation,
                "org.xen.xl.Error.Failed",
                "Error occurred executing the method");
        }
    } else {
        g_dbus_method_invocation_return_dbus_error (invocation,
            "org.xen.xl.Error.UnknownMethod",
            "Requested method is not implemented");
    }

clean_pipe:
    close(pipefds[0]);
    close(pipefds[1]);
clean_argv:
    g_strfreev(argv);
out:
    return;
}


static GVariant *handle_get_property(GDBusConnection  *connection,
                                     const gchar      *sender,
                                     const gchar      *object_path,
                                     const gchar      *interface_name,
                                     const gchar      *property_name,
                                     GError          **error,
                                     gpointer          user_data)
{
    GVariant *ret;

    ret = NULL;
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
       "No propterty such property %s", property_name);

  return ret;
}

static gboolean handle_set_property(GDBusConnection  *connection,
                                    const gchar      *sender,
                                    const gchar      *object_path,
                                    const gchar      *interface_name,
                                    const gchar      *property_name,
                                    GVariant         *value,
                                    GError          **error,
                                    gpointer          user_data)
{
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
       "No propterty such property %s", property_name);

    return *error == NULL;
}

static void on_bus_acquired(GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
    struct xl_dbus_service *svc = (struct xl_dbus_service *)user_data;

    svc->registration_id = g_dbus_connection_register_object(connection,
                                  dbus_root_obj,
                                  svc->introspection_data->interfaces[0],
                                  &svc->interface_vtable,
                                  user_data,
                                  NULL,
                                  NULL);
    g_assert(svc->registration_id > 0);
}

static void on_name_acquired(GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
}

static void on_name_lost(GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
    struct xl_dbus_service *svc = (struct xl_dbus_service *)user_data;

    g_main_loop_quit(svc->loop);
}

/*
GDBusErrorEntry xl_dbus_error_entries[] =
{
  { XL_UNKNOWN_METHOD, "org.xen.xl.Error.UnknownMethod" }
};
*/

int main_dbus(int argc, char **argv)
{
    struct xl_dbus_service *svc =
        (struct xl_dbus_service *)g_malloc(sizeof(struct xl_dbus_service));

    svc->interface_vtable.method_call = handle_method_call;
    svc->interface_vtable.get_property = handle_get_property;
    svc->interface_vtable.set_property = handle_set_property;

    svc->introspection_data =
        g_dbus_node_info_new_for_xml(introspection_xml, NULL);
    g_assert(svc->introspection_data != NULL);

    svc->owner_id = g_bus_own_name(G_BUS_TYPE_SYSTEM,
                             dbus_service_name,
                             G_BUS_NAME_OWNER_FLAGS_NONE,
                             on_bus_acquired,
                             on_name_acquired,
                             on_name_lost,
                             svc,
                             free_service);

    fprintf(stderr, "Registered dbus id: %d\n", svc->owner_id);
    svc->loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(svc->loop);

    g_bus_unown_name(svc->owner_id);

    return 0;
}
