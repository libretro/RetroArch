#include "pico/cyw43_arch.h"
#include "pico/stdio.h"
#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "smb2.h"
#include "libsmb2.h"

int smb2_ls_sync(char* user_url)
{
        struct smb2_context *smb2;
        struct smb2_url *url;
        struct smb2dir *dir;
        struct smb2dirent *ent;
        char link[1024];

	smb2 = smb2_init_context();
        if (smb2 == NULL) {
                printf("Failed to init context\n");
                exit(0);
        }

        url = smb2_parse_url(smb2, user_url);
        if (url == NULL) {
                printf("Failed to parse url: %s\n",
                        smb2_get_error(smb2));
                exit(0);
        }

        smb2_set_security_mode(smb2, SMB2_NEGOTIATE_SIGNING_ENABLED);

	if (smb2_connect_share(smb2, url->server, url->share, url->user) < 0) {
		printf("smb2_connect_share failed. %s\n", smb2_get_error(smb2));
		exit(10);
	}

	dir = smb2_opendir(smb2, url->path);
	if (dir == NULL) {
		printf("smb2_opendir failed. %s\n", smb2_get_error(smb2));
		exit(10);
	}

        while ((ent = smb2_readdir(smb2, dir))) {
                const char *type;
                time_t t;

                t = (time_t)ent->st.smb2_mtime;
                switch (ent->st.smb2_type) {
                case SMB2_TYPE_LINK:
                        type = "LINK";
                        break;
                case SMB2_TYPE_FILE:
                        type = "FILE";
                        break;
                case SMB2_TYPE_DIRECTORY:
                        type = "DIRECTORY";
                        break;
                default:
                        type = "unknown";
                        break;
                }
                printf("%-20s %-9s %15u\n", ent->name, type, ent->st.smb2_size);
                if (ent->st.smb2_type == SMB2_TYPE_LINK) {
                        char buf[256];

                        if (url->path && url->path[0]) {
                                sprintf(link, "%s/%s", url->path, ent->name);
                        } else {
                                sprintf(link, "%s", ent->name);
                        }
                        smb2_readlink(smb2, link, buf, 256);
                        printf("    -> [%s]\n", buf);
                }
        }

        smb2_closedir(smb2, dir);
        smb2_disconnect_share(smb2);
        smb2_destroy_url(url);
        smb2_destroy_context(smb2);
        
	return 0;
}

static void main_task(__unused void *params)
{
    if (cyw43_arch_init())
    {
        printf("Failed to initialise Pico W\n");
        return;
    }

    cyw43_arch_enable_sta_mode();
    
    printf("Connecting to WiFi...\n");

    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000))
    {
        printf("Failed to connect.\n");
        return;
    }

    ip4_addr_t *address = &(cyw43_state.netif[0].ip_addr);
    printf("Connected to %s as %d.%d.%d.%d as host %s\n", WIFI_SSID, ip4_addr1_16(address), ip4_addr2_16(address), ip4_addr3_16(address), ip4_addr4_16(address), cyw43_state.netif[0].hostname);

    smb2_ls_sync(SMB2_URL);
    vTaskDelete(NULL);
}

int main(void)
{
    TaskHandle_t task;

    stdio_init_all();

    xTaskCreate(main_task, "MainThread", configMINIMAL_STACK_SIZE, NULL, 1, &task);
    vTaskStartScheduler();

    return 0;
}
