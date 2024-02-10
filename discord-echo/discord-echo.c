/**
 * This file is part of Hercules.
 * http://herc.ws - http://github.com/HerculesWS/Hercules
 *
 * Copyright (C) 2013-2022 Hercules Dev Team
 * Copyright (C) 2020-2022 Andrei Karas (4144)
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

#include "common/hercules.h"
#include "common/memmgr.h"
#include "common/socket.h"
#include "map/pc.h"
#include "map/channel.h"
#include "plugins/HPMHooking.h"
#include "common/HPMDataCheck.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#define MAX_NAME_LENGTH  NAME_LENGTH
#define MAX_MESSAGE_LENGTH 250

struct PACKET_DISCORD_MESSAGE {
    int16_t packet_id;   // Packet ID (0x0F01)
    char channel[MAX_NAME_LENGTH];
    char username[MAX_NAME_LENGTH];
    char message[MAX_MESSAGE_LENGTH];
} __attribute__((packed));

void hercules_echo(struct channel_data *chan, struct map_session_data *sd, const char *msg) {
    if (strncmp(sd->status.name, "<", 1) == 0) return;

    CURL *curl = curl_easy_init();
    if (!curl) {
        ShowError("Failed to initialize CURL.\n");
        return;
    }

    const char *webhook_urls[] = {
        "https://discord.com/api/webhooks/webhook_id/webhook_token",    // main
        "https://discord.com/api/webhooks/webhook_id/webhook_token",    // trade
        "https://discord.com/api/webhooks/webhook_id/webhook_token"    // support
    };

    const char *webhook_url = NULL;
    if (strcmp(chan->name, "main") == 0) {
        webhook_url = webhook_urls[0];
    } else if (strcmp(chan->name, "trade") == 0) {
        webhook_url = webhook_urls[1];
    } else if (strcmp(chan->name, "support") == 0) {
        webhook_url = webhook_urls[2];
    } else {
        ShowWarning("Unknown webhook for channel: %s\n", chan->name);
        curl_easy_cleanup(curl);
        return;
    }

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    char postfields[2000];
    snprintf(postfields, sizeof(postfields), "{ \"username\": \"%s\", \"avatar_url\": \"https://i.imgur.com/4M34hi2.png\", \"content\": \"%s\" }", sd->status.name, msg);
    curl_easy_setopt(curl, CURLOPT_URL, webhook_url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postfields);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        ShowWarning("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    }

    curl_easy_cleanup(curl);
}

void discord_echo(int fd) {
    const struct PACKET_DISCORD_MESSAGE *packet = RP2PTR(fd);

    struct map_session_data *sd = malloc(sizeof(struct map_session_data));
    if (!sd) {
        ShowError("Failed to allocate memory for session data.\n");
        return;
    }

    safestrncpy(sd->status.name, packet->username, MAX_NAME_LENGTH);
    sd->fd = fd;

    struct channel_data *chan = channel->search(packet->channel, sd);
    if (!chan) {
        ShowWarning("Channel not found: %s\n", packet->channel);
        free(sd);
        return;
    }

    channel->send(chan, sd, packet->message);
    free(sd);
}

HPExport struct hplugin_info pinfo = {
    "discord-echo",
    SERVER_TYPE_MAP,
    "1.0",
    HPM_VERSION,
};

HPExport void plugin_init(void) {
    addPacket(0x0F01, 300, discord_echo, hpClif_Parse);
    addHookPost(channel, send, hercules_echo);
}
