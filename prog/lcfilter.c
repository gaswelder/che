
typedef {
	char pid[100];
	char appname[1000];
} proc_info_t;

typedef {
	char level;
	char tag[1000];
	char pid[100];
	char message[10000];
} log_entry_t;

bool streq(const char *a, const char *b) {
	return a && b && strcmp(a, b) == 0;
}

// Pretend to return new objects,
// but update and return the same static ones.
proc_info_t _proc = {};
log_entry_t _log = {};

int main(int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "Usage: lcfilter <application id>\n");
		return 1;
	}

	char line[10000] = {0};
	const char *appname = argv[1];

	// const char *appname = "com.foo.bar";
	// uint pid = 0;
	char pid[100] = "";
	
	while (fgets(line, sizeof(line), stdin)) {
		const log_entry_t *log = parse_line(line);
		if (!log) {
			fprintf(stderr, "couldn't parse log message: '%s'\n", line);
			continue;
		}

		//fprintf(stdout, "level=%c, tag=%s, pid=%s, msg=%s\n", log->level, log->tag, log->pid, log->message);

		// If a new instance of the process is started, update the id.
		const proc_info_t *proc = proc_info(log);
		if (proc) {
			if (streq(proc->appname, appname)) {
				strcpy(pid, proc->pid);
			}
		}

		if (!streq(log->pid, pid)) continue;

		printf("%s", line);
	}
}

/*
 * Matches start process message, returns process info.
 * Returns NULL if the line doesn't match.
 */
proc_info_t* proc_info(const log_entry_t *log) {
	// I/ActivityManager(  487): Start proc com.android.musicfx for broadcast com.android.musicfx/.ControlPanelReceiver: pid=2428 uid=10055 gids={50055, 3003, 3002, 1028}

	// The message should start with "Start proc".
	if (strstr(log->message, "Start proc ") != log->message) {
		return NULL;
	}

	size_t i = 0;
	const char *p = log->message + strlen("Start proc ");
	while (*p && !isspace(*p)) {
		_proc.appname[i] = *p;
		p++;
		i++;
	}
	_proc.appname[i] = 0;

	while (*p && *p != '=') {
		p++;
	}
	if (*p++ != '=') return NULL;
	
	i = 0;
	while (*p && isdigit(*p)) {
		_proc.pid[i++] = *p;
		p++;
	}
	_proc.pid[i] = 0;

	return &_proc;
}


/*
 * Matches a log message, returns log info.
 * Returns NULL if the line doesn't match.
 */
log_entry_t *parse_line(const char *line) {
	// D/PowerManagerService(  487): handleSandman: canDream=true, mWakefulness=Awake
	size_t i = 0;

	_log.level = *line++;
	if (*line++ != '/') return NULL;
	while (*line && *line != '(') {
		_log.tag[i++] = *line;
		line++;
	}
	_log.tag[i] = 0;

	if (*line++ != '(') return NULL;
	while (*line && *line == ' ') {
		line++;
	}
	
	i = 0;
	while (isdigit(*line)) {
		_log.pid[i++] = *line++;
	}
	_log.pid[i] = 0;

	if (*line++ != ')' || *line++ != ':' || *line++ != ' ') return NULL;

	i = 0;
	while (*line && *line != '\n' && *line != '\r') {
		_log.message[i++] = *line++;
	}
	_log.message[i] = 0;
	return &_log;
}
