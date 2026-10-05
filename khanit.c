#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct { // A struct to hold systemd and dinit fields to map them to each other cleanly with ref_map
    const char *systemd_key;
    const char *dinit_key;
} dir_map;

typedef struct { // Holds systemd and dinit field values to map them to each other cleanly with type_map
    const char *systemd_val;
    const char *dinit_val;
} val_map;

typedef struct { // Holds systemd time unit names and their values
    const char *unit_name;
    double multiplier;
} time_unit_map;

char *trim_space(char *str) {
    // Trim leading whitespace: move pointer forward
    while (isspace((unsigned char)*str)) {
        str++;
    }
    if (*str == '\0') {
        return str;
    }

    // Trim trailing whitespace: walk backward
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        end--;
    }
    *(end + 1) = '\0';

    return str;
}

const char *lookup_dinit_key(const char *systemd_key, dir_map ref_map[], size_t ref_map_size) { // Lookup function for systemd & dinit keys
    for (size_t i = 0; i < ref_map_size; i++) {
        if (strcmp(ref_map[i].systemd_key, systemd_key) == 0) {
            return ref_map[i].dinit_key;
        }
    }
    return nullptr;
}

const char *lookup_value(const char *systemd_val, val_map value_map[], size_t value_map_size) { // Generic lookup for any val_map table
    for (size_t i = 0; i < value_map_size; i++) {
        if (strcmp(value_map[i].systemd_val, systemd_val) == 0) {
            return value_map[i].dinit_val;
        }
    }
    return nullptr;
}

int main(int argc, char *argv[]) { // argc should equal 2: e.g. ./translator test.service

    if (argc != 2) {
        printf("Usage: %s <service-file>\n", argv[0]); // Error message if no filepath given
            return 1;
    }
    const char *service_filepath = argv[1]; // Second arg gives filepath
    FILE *fp = fopen(service_filepath,"r"); // Opens the file

    if (fp == NULL) {
        printf("Error: File %s could not be opened.\n", argv[1]);
        return 1;
    }

    char *line = nullptr;
    char output_filepath[512];
    snprintf(output_filepath, sizeof(output_filepath), "%s.dinit", service_filepath);
    FILE *output_fp = fopen(output_filepath, "w");

    if (output_fp == NULL) {
        printf("Error: Could not create output file: %s\n", output_filepath);
        fclose(fp);
        free(line);
        return 1;
    }
    size_t len = 0;
    bool section_header = false;

    dir_map ref_map[] = { // All pairs where the dinit field is nullptr means there's no clean conversion
    {"Documentation", nullptr},
    {"Group", nullptr},
    {"Type", "type"},
    {"Description", nullptr},
    {"Wants", "waits-for"},
    {"Requires", "depends-on"},
    {"Requisite", "depends-on"},
    {"BindsTo", "depends-on"},
    {"PartOf", "depends-on"},
    {"Upholds", "waits-for"},
    {"Before", "before"},
    {"After", "after"},
    {"ConditionPathExists", nullptr},
    {"ConditionPathExistsGlob", nullptr},
    {"ConditionVirtualization", nullptr},
    {"ConditionKernelCommandLine", nullptr},
    {"ConditionDirectoryNotEmpty", nullptr},
    {"ConditionFileNotEmpty", nullptr},
    {"ConditionFirstBoot", nullptr},
    {"ConditionArchitecture", nullptr},
    {"AssertPathExists", nullptr},
    {"AssertPathExistsGlob", nullptr},
    {"AssertVirtualization", nullptr},
    {"AssertKernelCommandLine", nullptr},
    {"AssertDirectoryNotEmpty", nullptr},
    {"AssertFileNotEmpty", nullptr},
    {"DefaultDependencies", nullptr},
    {"OnSuccess", "chain-to"},
    {"StartLimitBurst", "restart-limit-count"},
    {"StartLimitIntervalSec", "restart-limit-interval"},
    {"Alias", nullptr},
    {"WantedBy", "before"},
    {"RequiredBy", "before"},
    {"UpheldBy", "before"},
    {"PIDFile", "pid-file"},
    {"ExecStart", "command"},
    {"ExecStop", "stop-command"},
    {"TimeoutStartSec", "start-timeout"},
    {"TimeoutStopSec", "stop-timeout"},
    {"TimeoutSec", nullptr}, // Maps to start-timeout and stop timeout; will handle later
    {"Restart", "restart"},
    {"EnvironmentFile", "env-file"},
    {"User", "run-as"},
    {"WorkingDirectory", "working-dir"},
    {"LimitCORE", "rlimit-core"},
    {"LimitDATA", "rlimit-data"},
    {"LimitNOFILE", "rlimit-nofile"},
    {"UtmpIdentifier", "inittab-line"},
    {"KillSignal", "term-signal"},
    };

    val_map type_map[] = { // Maps systemd's [Type] values to dinit's type values
    {"simple", "process"},
    {"exec", "process"},
    {"forking", "bgprocess"},
    {"oneshot", "scripted"},
    {"notify", "process"},
    {"dbus", nullptr},
    };

    val_map restart_map[] = { // Maps systemd [Restart] values to dinit restart values
    {"no", "no"},
    {"always", "yes"}, // Closest match; dinit's "yes" is the broadest restart policy, practically same as "always"
    {"on-failure", "on-failure"},
    {"on-success", nullptr}, // No dinit equivalent
    {"on-abnormal", nullptr}, // No equivalent
    {"on-abort", nullptr}, // No equivalent
    {"on-watchdog", nullptr}, // No equivalent
    };

    val_map sig_map[] = { // Maps systemd SIG-prefix signal names to dinit unprefixed names (only dinit-supported signals included)
    {"SIGHUP", "HUP"},
    {"SIGINT", "INT"},
    {"SIGQUIT", "QUIT"},
    {"SIGKILL", "KILL"},
    {"SIGUSR1", "USR1"},
    {"SIGUSR2", "USR2"},
    {"SIGTERM", "TERM"},
    {"SIGCONT", "CONT"},
    {"SIGSTOP", "STOP"},
    };

    time_unit_map time_units[] = {
    {"s", 1},
    {"sec", 1},
    {"second", 1},
    {"seconds", 1},
    {"ms", 0.001},
    {"m", 60},
    {"min", 60},
    {"minute", 60},
    {"minutes", 60},
    {"h", 3600},
    {"hr", 3600},
    {"hour", 3600},
    {"hours", 3600},
    };
    
    while (getline(&line, &len, fp) != -1) {
        if (line[0] == '[') {
            section_header = true;
            printf("Section header found: %s", line);
        }
        else if (line[0] == '#' || line[0] == ';') {
            // comment line; skip
        }
        else if (line[0] == '\n') {
            // blank line; skip
        }
        else {
            section_header = false;
            char *equals_sign = strchr(line, '=');
            if (equals_sign != NULL) {
                *equals_sign = '\0'; // Split string at '='
                const char *key = trim_space(line);
                char *value = equals_sign + 1;
                char *newline = strchr(value, '\n');

                if (newline != NULL) {
                    *newline = '\0';
                }

                value = trim_space(value);
                const char *dinit_key = lookup_dinit_key(key, ref_map, sizeof(ref_map) / sizeof(ref_map[0]));
                if (dinit_key != NULL) {
                    if (strcmp(key, "Type") == 0) {
                        const char *dinit_value = lookup_value(value, type_map, sizeof(type_map) / sizeof(type_map[0]));
                        if (dinit_value != NULL) {
                            fprintf(output_fp, "%s=%s\n", dinit_key, dinit_value);
                        }
                        else {
                            fprintf(output_fp, "# Unsupported Type value: %s\n", value);
                        }
                    }
                    else if (strcmp(key, "Restart") == 0) {
                        const char *dinit_value = lookup_value(value, restart_map, sizeof(restart_map) / sizeof(restart_map[0]));
                        if (dinit_value != NULL) {
                            fprintf(output_fp, "%s=%s\n", dinit_key, dinit_value);
                        }
                        else {
                            fprintf(output_fp, "# Unsupported Restart value: %s\n", value);
                        }
                    }
                    else if (strcmp(key, "KillSignal") == 0) {
                        const char *dinit_value = lookup_value(value, sig_map, sizeof(sig_map) / sizeof(sig_map[0]));
                        if (dinit_value != NULL) {
                            fprintf(output_fp, "%s=%s\n", dinit_key, dinit_value);
                        }
                        else {
                            fprintf(output_fp, "# Unsupported KillSignal value: %s\n", value);
                        }
                    }
                    else {
                        fprintf(output_fp, "%s=%s\n", dinit_key, value);
                    }
                }
                else {
                    fprintf(output_fp, "# Unsupported or unknown key: %s\n", key);
                }
            }
        }
    }

    fclose(fp);
    fclose(output_fp);
    free(line);
    
    return 0;
}