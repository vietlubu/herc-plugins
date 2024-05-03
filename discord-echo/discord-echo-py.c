/**
 * This file is part of Hercules.
 * http://herc.ws - http://github.com/HerculesWS/Hercules
 *
 * Copyright (C) 2013-2022 Hercules Dev Team
 *
 * Hercules is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "common/hercules.h" /* Should always be the first Hercules file included! (if you don't make it first, you won't be able to use interfaces) */
#include "common/memmgr.h"
#include "common/socket.h"
#include "map/pc.h"
#include "map/channel.h"

#include "plugins/HPMHooking.h"
#include "common/HPMDataCheck.h" /* should always be the last Hercules file included! (if you don't make it last, it'll intentionally break compile time) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

HPExport struct hplugin_info pinfo = {
	"discord-echo",
	SERVER_TYPE_MAP,
	"1.0",
	HPM_VERSION,
};

struct PACKET_DISCORD_MESSAGE {
    int16 packet_id;   // Packet ID (0x0F01)
	char channel[NAME_LENGTH];
	char username[NAME_LENGTH];
	char message[250];
} __attribute__((packed));

void hercules_echo(struct channel_data *chan, struct map_session_data *sd, const char *msg) {
    if(strncmp(sd->status.name, "<", 1) == 0) return;
    
    CURL *curl;
    CURLcode res;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if(curl) {
        if (strcmp(chan->name, "main") == 0) {
            curl_easy_setopt(curl, CURLOPT_URL, "https://discord.com/api/webhooks/webhook_id/webhook_token");
        } else if (strcmp(chan->name, "trade") == 0) {
            curl_easy_setopt(curl, CURLOPT_URL, "https://discord.com/api/webhooks/webhook_id/webhook_token");
        } else if (strcmp(chan->name, "support") == 0) {
            curl_easy_setopt(curl, CURLOPT_URL, "https://discord.com/api/webhooks/webhook_id/webhook_token");
        } else {
            ShowWarning("Unknown webhook for channel: %s\n", chan->name);
            return;
        }
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        // curl_easy_setopt(curl, CURLOPT_URL, webhook_url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        char postfields[2000];
        sprintf(postfields, "{ \"username\": \"%s\", \"avatar_url\": \"https://i.imgur.com/4M34hi2.png\", \"content\": \"%s\" }", sd->status.name, msg);

        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postfields);

        res = curl_easy_perform(curl);

        if(res != CURLE_OK)
            ShowWarning("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));

        curl_easy_cleanup(curl);
    }

    curl_global_cleanup();
}

void discord_echo(int fd) {
    const struct PACKET_DISCORD_MESSAGE *packet = RP2PTR(fd);

    struct map_session_data *sd = malloc(sizeof(struct map_session_data));
    safestrncpy(sd->status.name, packet->username, sizeof(packet->username));
    sd->fd = fd;

	if( !sd ) {
        ShowError("discord_echo: Invalid session data!\n");
        return;
    }

    struct channel_data *chan = channel->search(packet->channel, sd);
    channel->send(chan, sd, packet->message);
}


// ========================================================================
// HPExport
// ========================================================================

/* run when server starts */
HPExport void plugin_init(void)
{
    addPacket(0x0F01, 300, discord_echo, hpClif_Parse);
    addHookPost(channel, send, hercules_echo);
}
