// monitor_server.c
// Compile: gcc monitor_server.c -o monitor_server -lpthread

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>

#define PORT 4444
#define INTERVAL 1

struct SysInfo {
    float cpu_usage;
    float cpu_temp;
    float ram_usage;
    float gpu_temp;
    float net_recv;
    float net_sent;
    float load_avg;
    float max_load;
};

float read_cpu_usage();
float read_cpu_temp();
float read_ram_usage();
float read_gpu_temp();
void read_net_usage(float* recv, float* sent);
float read_load_avg();

void* handle_client(void* arg) {
    int client_sock = *(int*)arg;
    static float max_load = 0;

    while (1) {
        struct SysInfo info;
        info.cpu_usage = read_cpu_usage();
        info.cpu_temp = read_cpu_temp();
        info.ram_usage = read_ram_usage();
        info.gpu_temp = read_gpu_temp();
        read_net_usage(&info.net_recv, &info.net_sent);
        info.load_avg = read_load_avg();
        if (info.load_avg > max_load) max_load = info.load_avg;
        info.max_load = max_load;

        send(client_sock, &info, sizeof(info), 0);
        sleep(INTERVAL);
    }

    close(client_sock);
    return NULL;
}

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size = sizeof(client_addr);

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_sock, 1);
    printf("[+] Serveur en attente sur le port %d...\n", PORT);

    while (1) {
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_size);
        printf("[+] Client connecté\n");
        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, &client_sock);
        pthread_detach(tid);
    }

    close(server_sock);
    return 0;
}

// -------------------- SYSTÈME -------------------------

float read_cpu_usage() {
    static long long last_user=0, last_nice=0, last_system=0, last_idle=0;
    FILE* f = fopen("/proc/stat", "r");
    char cpu[5]; long long user, nice, system, idle;
    fscanf(f, "%s %lld %lld %lld %lld", cpu, &user, &nice, &system, &idle);
    fclose(f);

    long long total = (user - last_user) + (nice - last_nice) + (system - last_system);
    long long total_time = total + (idle - last_idle);

    last_user = user; last_nice = nice; last_system = system; last_idle = idle;
    return (total_time == 0) ? 0 : (100.0f * total / total_time);
}

float read_cpu_temp() {
    FILE* f = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (!f) return 0;
    int temp;
    fscanf(f, "%d", &temp);
    fclose(f);
    return temp / 1000.0f;
}

float read_ram_usage() {
    FILE* f = fopen("/proc/meminfo", "r");
    long total, free, available;
    fscanf(f, "MemTotal: %ld kB\nMemFree: %ld kB\nMemAvailable: %ld kB", &total, &free, &available);
    fclose(f);
    return 100.0f * (total - available) / total;
}

float read_gpu_temp() {
    FILE* f = popen("nvidia-smi --query-gpu=temperature.gpu --format=csv,noheader,nounits 2>/dev/null", "r");
    if (!f) return 0;
    float temp;
    if (fscanf(f, "%f", &temp) != 1) temp = 0;
    pclose(f);
    return temp;
}

void read_net_usage(float* recv, float* sent) {
    static long long last_recv = 0, last_sent = 0;
    long long r, s;
    FILE* f = fopen("/proc/net/dev", "r");
    char buf[1024];
    *recv = *sent = 0;
    while (fgets(buf, sizeof(buf), f)) {
        if (strstr(buf, "eth0") || strstr(buf, "wlan0")) {
            sscanf(buf, "%*[^:]: %lld %*d %*d %*d %*d %*d %*d %*d %lld", &r, &s);
            break;
        }
    }
    fclose(f);
    *recv = (float)(r - last_recv) / 1024.0f;
    *sent = (float)(s - last_sent) / 1024.0f;
    last_recv = r;
    last_sent = s;
}

float read_load_avg() {
    FILE* f = fopen("/proc/loadavg", "r");
    float load;
    fscanf(f, "%f", &load);
    fclose(f);
    return load;
}
