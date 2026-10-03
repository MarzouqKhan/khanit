#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { // A struct to hold systemd and dinit fields to map them to each other cleanly with ref_map
    const char *systemd_key;
    const char *dinit_key;
} dir_map;

typedef struct { // Holds systemd and dinit field values to map them to each other cleanly with type_map
    const char *systemd_val;
    const char *dinit_val;
} val_map;

const char *lookup_dinit_key(const char *systemd_key, dir_map ref_map[], size_t ref_map_size) { // Lookup function for systemd & dinit keys
    for (size_t i = 0; i < ref_map_size; i++) {
        if (strcmp(ref_map[i].systemd_key, systemd_key) == 0) {
            return ref_map[i].dinit_key;
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
                const char *key = line;
                char *value = equals_sign + 1;
                char *newline = strchr(value, '\n');

                if (newline != NULL) {
                    *newline = '\0';
                }

                const char *dinit_key = lookup_dinit_key(key, ref_map, sizeof(ref_map) / sizeof(ref_map[0]));
                if (dinit_key != NULL) {
                    printf("%s=%s\n", dinit_key, value);
                }
                else {
                    printf("# Unsupported or unknown key: %s\n", key);
                }
            }
        }
    }

    fclose(fp);
    free(line);
    return 0;
}
