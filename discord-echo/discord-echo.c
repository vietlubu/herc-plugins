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
#include "api/jsonparser.h"
#include "plugins/HPMHooking.h"
#include "common/HPMDataCheck.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>
#include <concord/discord.h>

#define DISCORD_CONFIG "conf/import/discord.conf"

// PostHookChannelSendParams using to pass param to thread
struct PostHookChannelSendParams {
    struct channel_data *chan;
    struct map_session_data *sd;
    const char *msg;
};

// Define global structs and variables
struct discord *discord_client;
struct config_t config;
const char *discord_bot_token = NULL;
const struct config_setting_t *discord_channels = NULL;

/**
 * Function to get channel config by key and value
*/
struct config_setting_t *get_channel_config(const char *key, const char *value) {
    ShowInfo("key: %s - value: %s\n", key, value);
    struct config_setting_t *channel = NULL;
    for (int channel_index = 0; channel_index < libconfig->setting_length(discord_channels); channel_index++) {
        channel = libconfig->setting_get_elem(discord_channels, channel_index);
        const char *find_value = NULL;
        libconfig->lookup_string(&channel, key, &find_value);
        if (strcmp(find_value, value) == 0) {
            return channel;
        } else {
            channel = NULL;
        }
    }
    return channel;
}

/**
 * async function to send message in-game to discord
*/
void* async_hercules_echo(void *arg) {
    // Cast the void pointer to the correct type
    struct PostHookChannelSendParams *params = (struct PostHookChannelSendParams *)arg;

    // Access the parameters
    struct channel_data *chan = params->chan;
    struct map_session_data *sd = params->sd;
    const char *msg = params->msg;

    char *webhook_id_str = NULL;
    char *webhook_token = NULL;

    struct config_setting_t *find_channel = get_channel_config("ingame_channel", chan->name);
    if (find_channel != NULL) {
        libconfig->lookup_string(&find_channel, "discord_webhook_id", &webhook_id_str);
        libconfig->lookup_string(&find_channel, "discord_webhook_token", &webhook_token);
    }

    if (webhook_id_str == NULL || webhook_token == NULL) {
        ShowWarning("Cannot find channel conf for [%s]\n", chan->name);
        return NULL;
    }

    u64snowflake webhook_id = strtoull(webhook_id_str, NULL, 10);
    
    struct discord_ret ret = { .sync = true };
    struct discord_execute_webhook webhook_params = {
        // .avatar_url = "",
        .username = sd->status.name,
        .content = msg
    };
    discord_execute_webhook(discord_client, webhook_id, webhook_token, &webhook_params, &ret);

    return NULL;
}

/**
 * Function called after channel->send()
*/
void hercules_echo(struct channel_data *chan, struct map_session_data *sd, const char *msg) {
    // If character name starts with "<", it means that the message is from discord chat. Skip to call webhook
    // fd = 0 that mean that session is not from game client
    if (strncmp(sd->status.name, "<", 1) == 0 || sd->fd == 0) return;

    struct PostHookChannelSendParams params = {chan, sd, msg};
    // Create a new thread to execute the async_hercules_echo function
    pthread_t thread;
    if (pthread_create(&thread, NULL, async_hercules_echo, (void *)&params) != 0) {
        ShowError("hercules_echo: Error creating thread\n");
        return;
    }
}

/**
 * Logging after discor bot ready
*/
void discord_bot_on_ready(struct discord *discord_client, const struct discord_ready *event) {
    ShowStatus("discord_bot_on_ready: Logged in as %s!\n", event->user->username);
}

/**
 * Tracking discord has new message
*/
void discord_bot_on_message(struct discord *discord_client, const struct discord_message *event) {
    if (event->author->bot) return;

    char username[34];
    strcpy(username, "<");
    strcat(username, event->author->username);
    strcat(username, ">");

    struct map_session_data *sd = malloc(sizeof(struct map_session_data));
    sd->fd = 0;
    if (!sd) {
        ShowError("discord_bot_on_message: Failed to allocate memory for session data.\n");
        return;
    }

    safestrncpy(sd->status.name, username, NAME_LENGTH);

    char channel_id[20+1];
    sprintf(channel_id, "%llu", (unsigned long long)event->channel_id);
    
    if (channel_id == NULL) {
        ShowWarning("discord_bot_on_message: Channel ID is NULL\n");
        free(sd);
        return;
    }
    
    struct config_setting_t *find_channel = get_channel_config("discord_channel_id", channel_id);
    const char *channel_name = NULL;
    libconfig->lookup_string(&find_channel, "ingame_channel", &channel_name);
    if (!channel_name) {
        ShowWarning("discord_bot_on_message: Channel config not found: %s\n", event->channel_id);
        free(sd);
        return;
    }

    struct channel_data *chan = channel->search(channel_name, sd);

    if (!chan) {
        ShowWarning("discord_bot_on_message: Channel not found: %s\n", channel_name);
        free(sd);
        return;
    }

    channel->send(chan, sd, event->content);
    free(sd);
}

/**
 * async function to start discord bot
*/
void* async_start_discord_bot(void) {
    discord_client = discord_init(discord_bot_token);

    discord_add_intents(discord_client, DISCORD_GATEWAY_MESSAGE_CONTENT);
    discord_set_on_ready(discord_client, &discord_bot_on_ready);
    discord_set_on_message_create(discord_client, &discord_bot_on_message);
    discord_run(discord_client);
    return NULL;
}

HPExport struct hplugin_info pinfo = {
    "discord-echo",
    SERVER_TYPE_MAP,
    "0.1",
    HPM_VERSION,
};

HPExport void plugin_init(void) {
    addHookPost(channel, send, hercules_echo);
}

HPExport void server_online (void) {
    // Load discord configuration
    libconfig->load_file(&config, DISCORD_CONFIG);
    libconfig->lookup_string(&config, "discord_configuration/token", &discord_bot_token);
    discord_channels = libconfig->lookup(&config, "discord_configuration/channels");

    pthread_t discord_bot_thread;
    if (pthread_create(&discord_bot_thread, NULL, async_start_discord_bot, NULL) != 0) {
        ShowError("hercules_echo: Error creating discord_bot_thread\n");
        return;
    }
}

HPExport void server_post_final (void) {
    discord_shutdown(discord_client);
}
