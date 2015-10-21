#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/config.h"

static const char *config_libconfig =
	"watch-engine = \"inotify\";\n"
	"watch-path = [\"/tmp\", \"/var/lib/\"];\n"
	"watch-recursive = true;\n"
	"watch-update-nodes = true;\n"
	"watch-include = [\"\\.html$\", \"\\.txt$\"];\n"
	"watch-exclude = [\"^temp.txt$\"];\n"
	"watch-dirs-only = true;\n"
	"watch-files-only = true;\n"
	"kill-latency = 5.5;\n"
	"kill-signal = \"SIGINT\";\n"
	"start-latency = 1.5;\n"
	"redirect-input = true;\n"
	"redirect-output = \"/dev/null\";\n"
	"redirect-signal = [\"SIGUSR1\"];\n"
	"server-interface = \"eth0\";\n"
	"server-port = 20202;\n"
	"custom-test: {\n"
	"  filename = \"test\";\n"
	"  watch-files-only = false;\n"
	"}\n";

static const char *config_iniparser =
	"[ouroboros]\n"
	"watch-engine = inotify\n"
	"watch-path = /opt /var/lib\n"
	"watch-recursive = true\n"
	"watch-update-nodes = true\n"
	"watch-include = \\.net$ \\.ini$\n"
	"kill-latency = 2.5\n"
	"kill-signal = SIGKILL\n";

static char *mk_config_libconfig(void) {
	char *cfg = tempnam(NULL, NULL);
	FILE *f = fopen(cfg, "w");
	fwrite(config_libconfig, strlen(config_libconfig), 1, f);
	fclose(f);
	return cfg;
}

static char *mk_config_iniparser(void) {
	char *cfg = tempnam(NULL, NULL);
	FILE *f = fopen(cfg, "w");
	fwrite(config_iniparser, strlen(config_iniparser), 1, f);
	fclose(f);
	return cfg;
}

static void test_default_values(void) {

	struct ouroboros_config config;

	ouroboros_config_init(&config);

	assert(config.engine == ONT_POLL);
	assert(config.watch_recursive == 0);
	assert(config.watch_update_nodes == 0);
	assert(config.watch_dirs_only == 0);
	assert(config.watch_files_only == 0);
	assert(config.watch_paths == NULL);
	assert(config.watch_includes == NULL);
	assert(config.watch_excludes == NULL);
	assert(config.kill_signal == SIGTERM);
	assert(config.kill_latency == 1.0);
	assert(config.start_latency == 0.0);
	assert(config.redirect_input == 0);
	assert(config.redirect_output == NULL);
	assert(config.redirect_signals == NULL);
#if ENABLE_SERVER
	assert(config.server_iface == NULL);
	assert(config.server_port == 3945);
#endif

	/* check freeing resources when nothing was loaded */
	ouroboros_config_free(&config);

}

static void test_libconfig(void) {
#if ENABLE_LIBCONFIG

	struct ouroboros_config config = { 0 };

	char *file = mk_config_libconfig();
	int rv = load_ouroboros_config(file, "test", &config);
	unlink(file);

	assert(rv == 0);

#if HAVE_SYS_INOTIFY_H
	assert(config.engine == ONT_INOTIFY);
#else
	assert(config.engine == ONT_POLL);
#endif
	assert(config.watch_recursive == 1);
	assert(config.watch_update_nodes == 1);
	assert(config.watch_dirs_only == 1);
	/* this value is overwritten by the "custom" section */
	assert(config.watch_files_only == 0);
	assert(strcmp(config.watch_paths[0], "/tmp") == 0);
	assert(strcmp(config.watch_paths[1], "/var/lib/") == 0);
	assert(config.watch_paths[2] == NULL);
	assert(strcmp(config.watch_includes[0], "\\.html$") == 0);
	assert(strcmp(config.watch_includes[1], "\\.txt$") == 0);
	assert(config.watch_includes[2] == NULL);
	assert(strcmp(config.watch_excludes[0], "^temp.txt$") == 0);
	assert(config.watch_excludes[1] == NULL);
	assert(config.kill_signal == SIGINT);
	assert(config.kill_latency == 5.5);
	assert(config.start_latency == 1.5);
	assert(config.redirect_input == 1);
	assert(strcmp(config.redirect_output, "/dev/null") == 0);
	assert(config.redirect_signals[0] == SIGUSR1);
	assert(config.redirect_signals[1] == 0);
#if ENABLE_SERVER
	assert(strcmp(config.server_iface, "eth0") == 0);
	assert(config.server_port == 20202);
#endif

	ouroboros_config_free(&config);

	assert(config.watch_paths == NULL);
	assert(config.watch_includes == NULL);
	assert(config.watch_excludes == NULL);
	assert(config.redirect_output == NULL);
	assert(config.redirect_signals == NULL);
#if ENABLE_SERVER
	assert(config.server_iface == NULL);
#endif

#endif /* ENABLE_LIBCONFIG */
}

static void test_iniparser(void) {
#if ENABLE_INIPARSER

	struct ouroboros_config config = { 0 };

	char *file = mk_config_iniparser();
	int rv = load_ouroboros_ini_config(file, &config);
	unlink(file);

	assert(rv == 0);

#if HAVE_SYS_INOTIFY_H
	assert(config.engine == ONT_INOTIFY);
#else
	assert(config.engine == ONT_POLL);
#endif
	assert(config.watch_recursive == 1);
	assert(config.watch_update_nodes == 1);
	assert(config.watch_dirs_only == 0);
	assert(config.watch_files_only == 0);
	assert(strcmp(config.watch_paths[0], "/opt") == 0);
	assert(strcmp(config.watch_paths[1], "/var/lib") == 0);
	assert(config.watch_paths[2] == NULL);
	assert(strcmp(config.watch_includes[0], "\\.net$") == 0);
	assert(strcmp(config.watch_includes[1], "\\.ini$") == 0);
	assert(config.watch_includes[2] == NULL);
	assert(config.watch_excludes == NULL);
	assert(config.kill_signal == SIGKILL);
	assert(config.kill_latency == 2.5);
	assert(config.start_latency == 0.0);
	assert(config.redirect_input == 0);
	assert(config.redirect_output == NULL);
	assert(config.redirect_signals == NULL);
#if ENABLE_SERVER
	assert(config.server_iface == NULL);
	assert(config.server_port == 0);
#endif

#endif /* ENABLE_INIPARSER */
}

void main(void) {
	test_default_values();
	test_libconfig();
	test_iniparser();
}
