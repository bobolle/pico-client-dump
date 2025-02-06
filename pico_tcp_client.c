#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include <stdio.h>
#include "uss.h"

#include "lwip/tcp.h"
#include "lwip/pbuf.h"

uint8_t trigger_pin = 17;
uint8_t echo_pin = 16;
uint8_t green_light_pin = 15;
uint8_t red_light_pin = 14;

#define TCP_PORT 8080
#define BUF_SIZE 1024

typedef struct TCP_CLIENT_T_ {
    struct tcp_pcb *tcp_pcb;
    ip_addr_t remote_addr;
    uint8_t buffer[BUF_SIZE];
    int buffer_len;
    int sent_len;
    bool complete;
    int run_count;
    bool connected;
} TCP_CLIENT_T;

#if 0
static void dump_bytes(const uint8_t *bptr, uint32_t len) {
    unsigned int i = 0;
    printf("dump_bytes %d", len);
    for (i = 0; i < len;) {
        if ((i & 0x0f) == 0) {
            printf("\n");
        } else if ((i & 0x07) == 0) {
            printf(" ");
        } printf("%02x ", bptr[i++]);
    } printf("\n");
}
#define DUMP_BYTES dump_bytes
#else
#define DUMP_BYTES(A,B)
#endif

static TCP_CLIENT_T* tcp_client_init();
static bool tcp_client_open(void *arg);
static err_t tcp_client_close(void *arg);
static err_t tcp_client_connected(void *arg, struct tcp_pcb *tpcb, err_t err);
static err_t tcp_result(void *arg, int status);
static err_t tcp_client_poll(void *arg, struct tcp_pcb *tpcb);
static void tcp_client_err(void *arg, err_t err);
static err_t tcp_client_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static err_t tcp_client_sent(void *arg, struct tcp_pcb *tpcb, u16_t len);

int main() {
    stdio_init_all();

    if (cyw43_arch_init()) {
        printf("failed to initalise\n");
        return 1;
    }

    cyw43_arch_enable_sta_mode();

    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWD, CYW43_AUTH_WPA2_AES_PSK, 10000)) {
        printf("failed to connect\n");
        return 1;
    } else {
        printf("connected\n");
    }

    TCP_CLIENT_T *state = tcp_client_init();
    if (!state) {
        return 0;
    }

    if (!tcp_client_open(state)) {
        tcp_result(state, -1);
        return 0;
    }

    uss_init(trigger_pin, echo_pin);

    gpio_init(green_light_pin);
    gpio_init(red_light_pin);
    gpio_set_dir(green_light_pin, GPIO_OUT);
    gpio_set_dir(red_light_pin, GPIO_OUT);

    while (1) {
        //cyw43_arch_poll();
        //cyw43_arch_wait_for_work_until(make_timeout_time_ms(500));
    }

    free(state);
    cyw43_arch_deinit();

    return 0;
}

static TCP_CLIENT_T* tcp_client_init() {
    TCP_CLIENT_T *state = calloc(1, sizeof(TCP_CLIENT_T));
    if (!state) {
        printf("failed to allcoate state\n");
        return NULL;
    }

    ip4addr_aton("192.168.1.13", &state->remote_addr);

    return state;
}

static bool tcp_client_open(void *arg) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;
    printf("connecting to %s port %u\n", ip4addr_ntoa(&state->remote_addr), TCP_PORT);
    state->tcp_pcb = tcp_new_ip_type(IP_GET_TYPE(&state->remote_addr));

    if (!state->tcp_pcb) {
        printf("failed to create pcb\n");
        return false;
    }

    tcp_arg(state->tcp_pcb, state);
    tcp_poll(state->tcp_pcb, tcp_client_poll, 2);
    tcp_sent(state->tcp_pcb, tcp_client_sent);
    tcp_recv(state->tcp_pcb, tcp_client_recv);
    tcp_err(state->tcp_pcb, tcp_client_err);

    state->buffer_len = 0;

    cyw43_arch_lwip_begin();
    err_t err = tcp_connect(state->tcp_pcb, &state->remote_addr, TCP_PORT, tcp_client_connected);
    cyw43_arch_lwip_end();

    return true;
}

static err_t tcp_client_close(void *arg) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;

    err_t err = ERR_OK;

    if (state->tcp_pcb != NULL) {
        tcp_arg(state->tcp_pcb, NULL);
        tcp_poll(state->tcp_pcb, NULL, 0);
        tcp_sent(state->tcp_pcb, NULL);
        tcp_recv(state->tcp_pcb, NULL);
        tcp_err(state->tcp_pcb, NULL);

        err = tcp_close(state->tcp_pcb);

        if (err != ERR_OK) {
            printf("close failed %d, calling abort\n", err);
            tcp_abort(state->tcp_pcb);
            err = ERR_ABRT;
        }

        state->tcp_pcb = NULL;
    }

    return err;
}

static err_t tcp_client_connected(void *arg, struct tcp_pcb *tpcb, err_t err) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;
    if (err != ERR_OK) {
        printf("connect failed %d\n", err);
        return tcp_result(arg, err);
    }

    state->connected = true;
    printf("waiting for buffer from server\n");

    return ERR_OK;
}

static err_t tcp_result(void *arg, int status) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;

    if (status == 0) {
        printf("test_success\n");
    } else {
        printf("test failed %d\n", status);
    }

    state->complete = true;
    return tcp_client_close(arg);
}

static err_t tcp_client_poll(void *arg, struct tcp_pcb *tpcb) {
    TCP_CLIENT_T* state = (TCP_CLIENT_T*)arg;

    uint distance = get_pulse_cm(trigger_pin, echo_pin, 26100);

    if (distance == 0) {
        gpio_put(green_light_pin, 0);
        gpio_put(red_light_pin, 1);
    } else {
        gpio_put(green_light_pin, 1);
        gpio_put(red_light_pin, 0);
    }

    printf("distance in cm: %d\n", distance);

    char poll[24];

    snprintf(poll, sizeof(poll), "distance: %u",  distance);

    for (int i = 0; i < strlen(poll); i++) {
        state->buffer[i] = poll[i];
    }

    err_t err = tcp_write(tpcb, state->buffer, BUF_SIZE, TCP_WRITE_FLAG_COPY);

    if (err != ERR_OK) {
        printf("failed to write data");
        return tcp_result(arg, -1);
    }

    return ERR_OK;
}

static void tcp_client_err(void *arg, err_t err) {
    if (err != ERR_ABRT) {
        printf("tcp_client_err %d\n", err);
        tcp_result(arg, err);
    }
}

static err_t tcp_client_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;

    if (!p) {
        return tcp_result(arg, -1);
    }

    cyw43_arch_lwip_check();

    if (p->tot_len > 0) {
        printf("recv %d err %d\n", p->tot_len, err);

        for (struct pbuf *q = p; q != NULL; q = q->next) {
            DUMP_BYTES(q->payload, q->len);
        }

        const uint16_t buffer_left = BUF_SIZE - state->buffer_len;
        state->buffer_len += pbuf_copy_partial(p, state->buffer + state->buffer_len, p->tot_len > buffer_left ? buffer_left : p->tot_len, 0);

        tcp_recved(tpcb, p->tot_len);
    }

    pbuf_free(p);

    if (state->buffer_len == BUF_SIZE) {
        printf("writing %d bytes to server\n", state->buffer_len);
        err_t err = tcp_write(tpcb, state->buffer, state->buffer_len, TCP_WRITE_FLAG_COPY);

        if (err != ERR_OK) {
            printf("failed to write data %d\n", err);
            return tcp_result(arg, -1);
        }
    }

    return ERR_OK;
}

static err_t tcp_client_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;

    printf("tcp_client_sent %u\n", len);

    return ERR_OK;
}
