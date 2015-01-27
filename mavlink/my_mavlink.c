
#include <all.h>

#define TCP_BUFLEN 2000
#define UDP_BUFLEN 2042
#define UDP_PORT 14550

#ifndef WINDOWS
static struct sockaddr_in si_me, si_other;
static int s, slen = sizeof(si_other) , recv_len;
int mavlink_tcp_send (uint8_t *buf, uint16_t len);
#endif

LogList loglist[255];

static uint8_t udp_running = 0;
int16_t mission_max = -1;
int serial_fd_mavlink = -1;
ValueList MavLinkVars[MAVLINK_PARAMETER_MAX];
uint8_t mavlink_update_yaw = 0;
int c, res;
char serial_buf[255];
static uint32_t last_connection = 1;
static int8_t GPS_found = 0;
uint8_t mavlink_loghbeat = 0;
uint16_t mavlink_logs_total = 0;
uint16_t mavlink_logid = 0;
uint16_t mavlink_logstat = 0;
uint32_t mavlink_logreqsize = 0;
uint32_t mavlink_loggetsize = 0;
uint32_t mavlink_logstartstamp = 0;
uint16_t mavlink_timeout = 0;
uint16_t mavlink_maxparam = 0;
uint16_t mavlink_foundparam = 0;
uint8_t mavlink_udp_active = 0;
uint8_t mavlink_tcp_active = 0;
SDL_Thread *thread_udp = NULL;
SDL_Thread *thread_tcp = NULL;
int mavlink_udp (void *data);
int mavlink_tcp (void *data);
int param_timeout = 0;
int param_complete = 0;
int ahrs2_found = 0;

void mavlink_xml_save (FILE *fr) {
	int16_t n = 0;
	fprintf(fr, " <mavlink>\n");
	for (n = 0; n < MAVLINK_PARAMETER_MAX; n++) {
		if (MavLinkVars[n].name[0] != 0) {
			fprintf(fr, "  <param><name>%s</name><value>%f</value></param>\n", MavLinkVars[n].name, MavLinkVars[n].value);
		}
	}
	fprintf(fr, " </mavlink>\n");
}

uint8_t mavlink_init (char *port, uint32_t baud) {
	int n = 0;
	mavlink_maxparam = 0;
	mavlink_foundparam = 0;
	SDL_Log("mavlink: init serial port...\n");
	serial_fd_mavlink = serial_open(port, baud);
	for (n = 0; n < MAVLINK_PARAMETER_MAX; n++) {
		MavLinkVars[n].name[0] = 0;
		MavLinkVars[n].group[0] = 0;
		MavLinkVars[n].display[0] = 0;
		MavLinkVars[n].desc[0] = 0;
		MavLinkVars[n].values[0] = 0;
		MavLinkVars[n].bits[0] = 0;
		MavLinkVars[n].value = 0.0;
		MavLinkVars[n].onload = 0.0;
		MavLinkVars[n].type = MAV_VAR_FLOAT;
		MavLinkVars[n].id = -1;
	}

	udp_running = 1;
#ifdef SDL2
	thread_udp = SDL_CreateThread(mavlink_udp, NULL, NULL);
#else
	thread_udp = SDL_CreateThread(mavlink_udp, NULL);
#endif
	if ( thread_udp == NULL ) {
		fprintf(stderr, "mavlink: Thread konnte nicht gestartet werden (mavlink_udp): %s\n", SDL_GetError());
	}

#ifdef SDL2
	thread_tcp = SDL_CreateThread(mavlink_tcp, NULL, NULL);
#else
	thread_tcp = SDL_CreateThread(mavlink_tcp, NULL);
#endif
	if ( thread_tcp == NULL ) {
		fprintf(stderr, "mavlink: Thread konnte nicht gestartet werden (mavlink_tcp): %s\n", SDL_GetError());
	}

	return 0;
}

void mavlink_exit (void) {
	udp_running = 0;
	if (thread_udp != NULL) {
		SDL_Log("mavlink: wait udp thread\n");
		SDL_WaitThread(thread_udp, NULL);
		thread_udp = NULL;
	}
	if (thread_tcp != NULL) {
		SDL_Log("mavlink: wait tcp thread\n");
		SDL_WaitThread(thread_tcp, NULL);
		thread_tcp = NULL;
	}
	if (serial_fd_mavlink >= 0) {
		serial_close(serial_fd_mavlink);
		serial_fd_mavlink = -1;
	}
}

void mavlink_stop_feeds (void) {
	SDL_Log("mavlink: stopping feeds!\n");
	mavlink_message_t msg1;
	mavlink_msg_request_data_stream_pack(127, 0, &msg1, ModelData[ModelActive].sysid, ModelData[ModelActive].compid, MAV_DATA_STREAM_ALL, 0, 0);
	mavlink_send_message(&msg1);
}

void mavlink_send_value (char *name, float val, int8_t type) {
	mavlink_message_t msg;
	if (type == -1) {
		type = MAV_VAR_FLOAT;
	}
	mavlink_msg_param_set_pack(127, 0, &msg, ModelData[ModelActive].sysid, ModelData[ModelActive].compid, name, val, type);
	if (clientmode == 1) {
#ifndef WINDOWS
		webclient_send_value(clientmode_server, clientmode_port, name, val, type);
#endif
	} else {
		mavlink_send_message(&msg);
		ModelData[ModelActive].mavlink_update = (int)time(0);
	}
}

void mavlink_set_value (char *name, float value, int8_t type, int16_t id) {
	uint16_t n = 0;
	uint8_t flag = 0;
	float min = 999999.0;
	float max = 999999.0;
	for (n = 0; n < MAVLINK_PARAMETER_MAX; n++) {
		if (strcmp(MavLinkVars[n].name, name) == 0 && (MavLinkVars[n].id == id || MavLinkVars[n].id == -1 || id > MAVLINK_PARAMETER_MAX || id == -1)) {
			MavLinkVars[n].value = value;
			MavLinkVars[n].id = id;
			if (type == -1 || type > 6) {
				type = MAV_VAR_FLOAT;
			}
			MavLinkVars[n].type = type;
			flag = 1;
			break;
		}
	}
	if (flag == 0) {
		for (n = 0; n < MAVLINK_PARAMETER_MAX; n++) {
			if (MavLinkVars[n].name[0] == 0) {
				if (type == MAV_VAR_FLOAT) {
					min = -999999.0;
					max = 999999.0;
				} else if (type == MAV_VAR_UINT8) {
					min = 0.0;
					max = 255.0;
				} else if (type == MAV_VAR_INT8) {
					min = -127.0;
					max = 127.0;
				} else if (type == MAV_VAR_UINT16) {
					min = 0.0;
					max = 65535.0;
				} else if (type == MAV_VAR_INT16) {
					min = -32767.0;
					max = 32767.0;
				} else if (type == MAV_VAR_UINT32) {
					min = 0.0;
					max = 999999.0;
				} else if (type == MAV_VAR_INT32) {
					min = -999999.0;
					max = 999999.0;
				} else {
					min = -999999.0;
					max = 999999.0;
					type = MAV_VAR_FLOAT;
				}
				if (strstr(name, "baud") > 0) {
					min = 1200.0;
					max = 115200.0;
				}
				strncpy(MavLinkVars[n].name, name, 17);
				MavLinkVars[n].value = value;
				MavLinkVars[n].onload = value;
				MavLinkVars[n].id = id;

				if (type == -1) {
					type = MAV_VAR_FLOAT;
				}
				//printf("###### %s ## %i ###\n", name, type);

				MavLinkVars[n].type = type;
				MavLinkVars[n].min = min;
				MavLinkVars[n].max = max;
				if (MavLinkVars[n].min > MavLinkVars[n].value) {
					MavLinkVars[n].min = MavLinkVars[n].value;
				}
				if (MavLinkVars[n].max < MavLinkVars[n].value) {
					MavLinkVars[n].max = MavLinkVars[n].value;
				}

				if (strncmp(MavLinkVars[n].name, "RC", 2) == 0) {
					strncpy(MavLinkVars[n].group, "RC", 17);
				} else if (strncmp(MavLinkVars[n].name, "CH", 2) == 0) {
					strncpy(MavLinkVars[n].group, "Channels", 17);
				} else if (strncmp(MavLinkVars[n].name, "FLTMODE", 7) == 0) {
					strncpy(MavLinkVars[n].group, "FlightMode", 17);
				} else if (strncmp(MavLinkVars[n].name, "COMPASS", 7) == 0) {
					strncpy(MavLinkVars[n].group, "Compass", 17);
				} else if (strncmp(MavLinkVars[n].name, "MAG", 3) == 0) {
					strncpy(MavLinkVars[n].group, "Compass", 17);
				} else if (strncmp(MavLinkVars[n].name, "SR", 2) == 0) {
					strncpy(MavLinkVars[n].group, "Serial", 17);
				} else if (strncmp(MavLinkVars[n].name, "TELEM", 5) == 0) {
					strncpy(MavLinkVars[n].group, "Serial", 17);
				} else if (strncmp(MavLinkVars[n].name, "SERIAL", 6) == 0) {
					strncpy(MavLinkVars[n].group, "Serial", 17);
				} else if (strncmp(MavLinkVars[n].name, "GPS", 3) == 0) {
					strncpy(MavLinkVars[n].group, "GPS", 17);
				} else if (strncmp(MavLinkVars[n].name, "MNT", 3) == 0) {
					strncpy(MavLinkVars[n].group, "Gimbal", 17);
				} else if (strncmp(MavLinkVars[n].name, "CAM", 3) == 0) {
					strncpy(MavLinkVars[n].group, "Gimbal", 17);
				} else if (strncmp(MavLinkVars[n].name, "BARO", 4) == 0) {
					strncpy(MavLinkVars[n].group, "Baro", 17);
				} else if (strncmp(MavLinkVars[n].name, "THR_", 4) == 0) {
					strncpy(MavLinkVars[n].group, "Throttle", 17);
				} else if (strncmp(MavLinkVars[n].name, "SUPER_SIMPLE", 12) == 0) {
					strncpy(MavLinkVars[n].group, "Simple", 17);
				} else if (strncmp(MavLinkVars[n].name, "SIMPLE", 6) == 0) {
					strncpy(MavLinkVars[n].group, "Simple", 17);
				} else if (strncmp(MavLinkVars[n].name, "INAV", 4) == 0) {
					strncpy(MavLinkVars[n].group, "Navigation", 17);
				} else if (strncmp(MavLinkVars[n].name, "WPNAV", 5) == 0) {
					strncpy(MavLinkVars[n].group, "Navigation", 17);
				} else if (strncmp(MavLinkVars[n].name, "WP_", 3) == 0) {
					strncpy(MavLinkVars[n].group, "Navigation", 17);
				}
				break;
			}
		}
	}
	ModelData[ModelActive].mavlink_update = (int)time(0);
}

void mavlink_handleMessage(mavlink_message_t* msg) {
	mavlink_message_t msg2;
	char sysmsg_str[1024];
	if (param_complete == 0) {
		if (param_timeout > 1) {
			param_timeout--;
		} else if (param_timeout == 1) {
			param_timeout = 10;
			int n = 0;
			int n2 = 0;
			int flag2 = 0;
			for (n2 = 0; n2 < mavlink_maxparam; n2++) {
				int flag = 0;
				for (n = 0; n < MAVLINK_PARAMETER_MAX; n++) {
					if (MavLinkVars[n].name[0] != 0 && MavLinkVars[n].id == n2) {
						flag = 1;
						break;
					}
				}
				if (flag == 0) {
					printf("# parameter %i not found #\n", n2);
					mavlink_param_get_id(n2);
					flag2 = 1;
					break;
				}
			}
			if (flag2 == 0) {
				printf("# parameter complete #\n");
				param_complete = 1;
			}
		}
	}

	//SDL_Log("mavlink: ## MAVLINK_MSG_ID %i ##\n", msg->msgid);

	switch (msg->msgid) {
		case MAVLINK_MSG_ID_HEARTBEAT: {
			mavlink_heartbeat_t packet;
			mavlink_msg_heartbeat_decode(msg, &packet);
			ModelData[ModelActive].dronetype = packet.type;
			ModelData[ModelActive].pilottype = packet.autopilot;
//			ModelData[ModelActive].mode = (packet.base_mode & 0x80)>7;

			ModelData[ModelActive].mode = packet.custom_mode;

			if (packet.system_status == MAV_STATE_ACTIVE) {
				ModelData[ModelActive].armed = MODEL_ARMED;
			} else {
				ModelData[ModelActive].armed = MODEL_DISARMED;
			}
//			SDL_Log("Heartbeat: %i, %i, %i\n", ModelData[ModelActive].armed, ModelData[ModelActive].mode, ModelData[ModelActive].status);
			ModelData[ModelActive].heartbeat = 100;
//			sprintf(sysmsg_str, "Heartbeat: %i", (int)time(0));
			if ((*msg).sysid != 0xff) {
				ModelData[ModelActive].sysid = (*msg).sysid;
				ModelData[ModelActive].compid = (*msg).compid;
				if (mavlink_maxparam == 0) {
					mavlink_start_feeds();
				}
			}
//			redraw_flag = 1;

			mavlink_msg_heartbeat_pack(127, 0, &msg2, MAV_TYPE_GCS, MAV_AUTOPILOT_INVALID, 0, 0, 0);
			mavlink_send_message(&msg2);

			break;
		}
		case MAVLINK_MSG_ID_RC_CHANNELS_SCALED: {
			mavlink_rc_channels_scaled_t packet;
			mavlink_msg_rc_channels_scaled_decode(msg, &packet);
//			SDL_Log("Radio: %i,%i,%i\n", packet.chan1_scaled, packet.chan2_scaled, packet.chan3_scaled);

/*			if ((int)packet.chan6_scaled > 1000) {
				mode = MODE_MISSION;
			} else if ((int)packet.chan6_scaled < -1000) {
				mode = MODE_MANUEL;
			} else {
				mode = MODE_POSHOLD;
			}
			if ((int)packet.chan7_scaled > 1000) {
				mode = MODE_RTL;
			} else if ((int)packet.chan7_scaled < -1000) {
				mode = MODE_SETHOME;
			}
*/

			ModelData[ModelActive].radio[0] = (int)packet.chan1_scaled / 100.0;
			ModelData[ModelActive].radio[1] = (int)packet.chan2_scaled / 100.0;
			ModelData[ModelActive].radio[2] = (int)packet.chan3_scaled / 100.0;
			ModelData[ModelActive].radio[3] = (int)packet.chan4_scaled / 100.0;
			ModelData[ModelActive].radio[4] = (int)packet.chan5_scaled / 100.0;
			ModelData[ModelActive].radio[5] = (int)packet.chan6_scaled / 100.0;
			ModelData[ModelActive].radio[6] = (int)packet.chan7_scaled / 100.0;
			ModelData[ModelActive].radio[7] = (int)packet.chan8_scaled / 100.0;

//			redraw_flag = 1;
			break;
		}
		case MAVLINK_MSG_ID_SCALED_PRESSURE: {
			mavlink_scaled_pressure_t packet;
			mavlink_msg_scaled_pressure_decode(msg, &packet);
//			SDL_Log("BAR;%i;%0.2f;%0.2f;%0.2f\n", time(0), packet.press_abs, packet.press_diff, packet.temperature / 100.0);
//			redraw_flag = 1;
			break;
		}
		case MAVLINK_MSG_ID_ATTITUDE: {
			mavlink_attitude_t packet;
			mavlink_msg_attitude_decode(msg, &packet);
			if (ahrs2_found == 0) {
				ModelData[ModelActive].roll = toDeg(packet.roll);
				ModelData[ModelActive].pitch = toDeg(packet.pitch);
			}
			if (toDeg(packet.yaw) < 0.0) {
				ModelData[ModelActive].yaw = 360.0 + toDeg(packet.yaw);
			} else {
				ModelData[ModelActive].yaw = toDeg(packet.yaw);
			}
			mavlink_update_yaw = 1;
//			SDL_Log("ATT;%i;%0.2f;%0.2f;%0.2f\n", time(0), toDeg(packet.roll), toDeg(packet.pitch), toDeg(packet.yaw));
			redraw_flag = 1;
			break;
		}
		case MAVLINK_MSG_ID_SCALED_IMU: {
//			SDL_Log("SCALED_IMU\n");
			break;
		}
		case MAVLINK_MSG_ID_GPS_RAW_INT: {
			mavlink_gps_raw_int_t packet;
			mavlink_msg_gps_raw_int_decode(msg, &packet);
			if (packet.lat != 0 && packet.lon != 0) {
				ModelData[ModelActive].p_lat = (float)packet.lat / 10000000.0;
				ModelData[ModelActive].p_long = (float)packet.lon / 10000000.0;
				ModelData[ModelActive].speed = (float)packet.vel / 100.0;
				ModelData[ModelActive].numSat = packet.satellites_visible;
				ModelData[ModelActive].gpsfix = packet.fix_type;
				ModelData[ModelActive].hdop = (float)packet.eph / 100.0;
				ModelData[ModelActive].vdop = (float)packet.epv / 100.0;
				if (ModelData[ModelActive].gpsfix > 2) {
					GPS_found = 1;
					ModelData[ModelActive].p_alt = (float)packet.alt / 1000.0;
				}
				redraw_flag = 1;
			}
			break;
		}
		case MAVLINK_MSG_ID_RC_CHANNELS_RAW: {
			mavlink_rc_channels_raw_t packet;
			mavlink_msg_rc_channels_raw_decode(msg, &packet);

			ModelData[ModelActive].radio_raw[0] = (int)packet.chan1_raw;
			ModelData[ModelActive].radio_raw[1] = (int)packet.chan2_raw;
			ModelData[ModelActive].radio_raw[2] = (int)packet.chan3_raw;
			ModelData[ModelActive].radio_raw[3] = (int)packet.chan4_raw;
			ModelData[ModelActive].radio_raw[4] = (int)packet.chan5_raw;
			ModelData[ModelActive].radio_raw[5] = (int)packet.chan6_raw;
			ModelData[ModelActive].radio_raw[6] = (int)packet.chan7_raw;
			ModelData[ModelActive].radio_raw[7] = (int)packet.chan8_raw;

			ModelData[ModelActive].radio[0] = (int)packet.chan1_raw / 5 - 300;
			ModelData[ModelActive].radio[1] = (int)packet.chan2_raw / 5 - 300;
			ModelData[ModelActive].radio[2] = (int)packet.chan3_raw / 5 - 300;
			ModelData[ModelActive].radio[3] = (int)packet.chan4_raw / 5 - 300;
			ModelData[ModelActive].radio[4] = (int)packet.chan5_raw / 5 - 300;
			ModelData[ModelActive].radio[5] = (int)packet.chan6_raw / 5 - 300;
			ModelData[ModelActive].radio[6] = (int)packet.chan7_raw / 5 - 300;
			ModelData[ModelActive].radio[7] = (int)packet.chan8_raw / 5 - 300;
			ModelData[ModelActive].rssi_rc_rx = (int)packet.rssi * 100 / 255;
			break;
		}
		case MAVLINK_MSG_ID_SERVO_OUTPUT_RAW: {
//			SDL_Log("SERVO_OUTPUT_RAW\n");
			break;
		}
		case MAVLINK_MSG_ID_SYS_STATUS: {
			mavlink_sys_status_t packet;
			mavlink_msg_sys_status_decode(msg, &packet);
//			SDL_Log("%0.1f %%, %0.3f V)\n", packet.load / 10.0, packet.voltage_battery / 1000.0);
			ModelData[ModelActive].voltage = packet.voltage_battery / 1000.0;
			ModelData[ModelActive].ampere = (float)packet.current_battery / 100.0;
			ModelData[ModelActive].load = packet.load / 10.0;
//			redraw_flag = 1;
			break;
		}
		case MAVLINK_MSG_ID_STATUSTEXT: {
			mavlink_statustext_t packet;
			mavlink_msg_statustext_decode(msg, &packet);
			SDL_Log("mavlink: ## %s ##\n", packet.text);
			sys_message((char *)packet.text);
//			redraw_flag = 1;
			break;
		}
		case MAVLINK_MSG_ID_PARAM_VALUE: {
			mavlink_param_value_t packet;
			mavlink_msg_param_value_decode(msg, &packet);
			char var[101];
			uint16_t n1 = 0;
			uint16_t n2 = 0;
			for (n1 = 0; n1 < strlen(packet.param_id); n1++) {
				if (packet.param_id[n1] != 9 && packet.param_id[n1] != ' ' && packet.param_id[n1] != '\t') {
					var[n2++] = packet.param_id[n1];
				}
			}
			var[n2++] = 0;
//strcpy(var, packet.param_id);

//	MAV_VAR_FLOAT=0, /* 32 bit float | */
//	MAV_VAR_UINT8=1, /* 8 bit unsigned integer | */
//	MAV_VAR_INT8=2, /* 8 bit signed integer | */
//	MAV_VAR_UINT16=3, /* 16 bit unsigned integer | */
//	MAV_VAR_INT16=4, /* 16 bit signed integer | */
//	MAV_VAR_UINT32=5, /* 32 bit unsigned integer | */
//	MAV_VAR_INT32=6, /* 32 bit signed integer | */

//mavlink_param_get_id (uint16_t id);

			sprintf(sysmsg_str, "PARAM_VALUE (%i/%i): #%s#%s# = %f (Type: %i)", packet.param_index + 1, packet.param_count, var, packet.param_id, packet.param_value, packet.param_type);
			SDL_Log("mavlink: %s\n", sysmsg_str);
			sys_message(sysmsg_str);
			mavlink_maxparam = packet.param_count;
			mavlink_timeout = 0;

			mavlink_set_value(var, packet.param_value, packet.param_type, packet.param_index);

			if (packet.param_index + 1 == packet.param_count || packet.param_index % 10 == 0) {
				mavlink_param_xml_meta_load();
			}

			param_timeout = 10;

//			redraw_flag = 1;
			break;
		}
		case MAVLINK_MSG_ID_MISSION_COUNT: {
			mavlink_mission_count_t packet;
			mavlink_msg_mission_count_decode(msg, &packet);
			sprintf(sysmsg_str, "MISSION_COUNT: %i\n", packet.count);
			sys_message(sysmsg_str);
			mission_max = packet.count;
			if (mission_max > 0) {
				mavlink_msg_mission_request_pack(127, 0, &msg2, ModelData[ModelActive].sysid, ModelData[ModelActive].compid, 0);
				mavlink_send_message(&msg2);
			}
//			redraw_flag = 1;
			break;
		}
		case MAVLINK_MSG_ID_MISSION_ACK: {
			SDL_Log("mavlink: Mission-Transfer ACK\n");
			break;
		}
		case MAVLINK_MSG_ID_MISSION_REQUEST: {
			mavlink_mission_request_t packet;
			mavlink_msg_mission_request_decode(msg, &packet);
			uint16_t id = packet.seq;
			uint16_t id2 = packet.seq;
			uint16_t type = 0;

			if (ModelData[ModelActive].teletype == TELETYPE_MEGAPIRATE_NG || ModelData[ModelActive].teletype == TELETYPE_ARDUPILOT) {
				if (id2 > 0) {
					id2 = id2 - 1;
				}
			}

			sprintf(sysmsg_str, "sending Waypoint (%i): %s\n", id, WayPoints[id2 + 1].name);
			sys_message(sysmsg_str);
			if (strcmp(WayPoints[id2 + 1].command, "WAYPOINT") == 0) {
				SDL_Log("mavlink: Type: MAV_CMD_NAV_WAYPOINT\n");
				type = MAV_CMD_NAV_WAYPOINT;
			} else if (strcmp(WayPoints[id2 + 1].command, "RTL") == 0) {
				SDL_Log("mavlink: Type: MAV_CMD_NAV_RETURN_TO_LAUNCH\n");
				type = MAV_CMD_NAV_RETURN_TO_LAUNCH;
			} else if (strcmp(WayPoints[id2 + 1].command, "LAND") == 0) {
				SDL_Log("mavlink: Type: MAV_CMD_NAV_LAND\n");
				type = MAV_CMD_NAV_LAND;
			} else if (strcmp(WayPoints[id2 + 1].command, "TAKEOFF") == 0) {
				SDL_Log("mavlink: Type: MAV_CMD_NAV_TAKEOFF\n");
				type = MAV_CMD_NAV_TAKEOFF;
			} else if (strcmp(WayPoints[id2 + 1].command, "SHUTTER") == 0) {
				SDL_Log("mavlink: Type: MAV_CMD_DO_DIGICAM_CONTROL\n");
				type = MAV_CMD_DO_DIGICAM_CONTROL;
			} else if (strcmp(WayPoints[id2 + 1].command, "SHUTTER_INT") == 0) {
				SDL_Log("mavlink: Type: MAV_CMD_DO_SET_CAM_TRIGG_DIST\n");
				type = MAV_CMD_DO_SET_CAM_TRIGG_DIST;
			} else if (strcmp(WayPoints[id2 + 1].command, "RELAY") == 0) {
				SDL_Log("mavlink: Type: MAV_CMD_DO_SET_RELAY\n");
				type = MAV_CMD_DO_SET_RELAY;
			} else if (strcmp(WayPoints[id2 + 1].command, "RELAY_REP") == 0) {
				SDL_Log("mavlink: Type: MAV_CMD_DO_REPEAT_RELAY\n");
				type = MAV_CMD_DO_REPEAT_RELAY;
			} else if (strcmp(WayPoints[id2 + 1].command, "SERVO") == 0) {
				SDL_Log("mavlink: Type: MAV_CMD_DO_SET_SERVO\n");
				type = MAV_CMD_DO_SET_SERVO;
			} else if (strcmp(WayPoints[id2 + 1].command, "SERVO_REP") == 0) {
				SDL_Log("mavlink: Type: MAV_CMD_DO_REPEAT_SERVO\n");
				type = MAV_CMD_DO_REPEAT_SERVO;
			} else if (strcmp(WayPoints[id2 + 1].command, "SET_ROI") == 0) {
				SDL_Log("mavlink: Type: MAV_CMD_NAV_ROI\n");
				type = MAV_CMD_NAV_ROI;
				type = 201;
			} else {
				SDL_Log("mavlink: Type: UNKNOWN\n");
				type = MAV_CMD_NAV_WAYPOINT;
			}

			sprintf(sysmsg_str, "SENDING MISSION_ITEM: %i: %f, %f, %f\n", id, WayPoints[id2 + 1].p_lat, WayPoints[id2 + 1].p_long, WayPoints[id2 + 1].p_alt);
			SDL_Log("mavlink: %s\n", sysmsg_str);


//	MAV_FRAME_GLOBAL=0, /* Global coordinate frame, WGS84 coordinate system. First value / x: latitude, second value / y: longitude, third value / z: positive altitude over mean sea level (MSL) | */
//	MAV_FRAME_LOCAL_NED=1, /* Local coordinate frame, Z-up (x: north, y: east, z: down). | */
//	MAV_FRAME_MISSION=2, /* NOT a coordinate frame, indicates a mission command. | */
//	MAV_FRAME_GLOBAL_RELATIVE_ALT=3, /* Global coordinate frame, WGS84 coordinate system, relative altitude over ground with respect to the home position. First value / x: latitude, second value / y: longitude, third value / z: positive altitude with 0 being at the altitude of the home location. | */
//	MAV_FRAME_LOCAL_ENU=4, /* Local coordinate frame, Z-down (x: east, y: north, z: up) | */
//	MAV_FRAME_GLOBAL_INT=5, /* Global coordinate frame, WGS84 coordinate system. First value / x: latitude in degrees*1.0e-7, second value / y: longitude in degrees*1.0e-7, third value / z: positive altitude over mean sea level (MSL) | */
//	MAV_FRAME_GLOBAL_RELATIVE_ALT_INT=6, /* Global coordinate frame, WGS84 coordinate system, relative altitude over ground with respect to the home position. First value / x: latitude in degrees*10e-7, second value / y: longitude in degrees*10e-7, third value / z: positive altitude with 0 being at the altitude of the home location. | */
//	MAV_FRAME_LOCAL_OFFSET_NED=7, /* Offset to the current local frame. Anything expressed in this frame should be added to the current local frame position. | */
//	MAV_FRAME_BODY_NED=8, /* Setpoint in body NED frame. This makes sense if all position control is externalized - e.g. useful to command 2 m/s^2 acceleration to the right. | */
//	MAV_FRAME_BODY_OFFSET_NED=9, /* Offset in body NED frame. This makes sense if adding setpoints to the current flight path, to avoid an obstacle - e.g. useful to command 2 m/s^2 acceleration to the east. | */
//	MAV_FRAME_GLOBAL_TERRAIN_ALT=10, /* Global coordinate frame with above terrain level altitude. WGS84 coordinate system, relative altitude over terrain with respect to the waypoint coordinate. First value / x: latitude in degrees, second value / y: longitude in degrees, third value / z: positive altitude in meters with 0 being at ground level in terrain model. | */
//	MAV_FRAME_GLOBAL_TERRAIN_ALT_INT=11, /* Global coordinate frame with above terrain level altitude. WGS84 coordinate system, relative altitude over terrain with respect to the waypoint coordinate. First value / x: latitude in degrees*10e-7, second value / y: longitude in degrees*10e-7, third value / z: positive altitude in meters with 0 being at ground level in terrain model. | */


			if (ModelData[ModelActive].dronetype != MAV_TYPE_FIXED_WING && WayPoints[id2 + 1].frametype == MAV_FRAME_GLOBAL) {
				SDL_Log("mavlink: copter absolut alt workaround");
				mavlink_msg_mission_item_pack(127, 0, &msg2, ModelData[ModelActive].sysid, ModelData[ModelActive].compid, id, WayPoints[id2 + 1].frametype, type, 0, 1, WayPoints[id2 + 1].param1, WayPoints[id2 + 1].param2, WayPoints[id2 + 1].param3, WayPoints[id2 + 1].param4, WayPoints[id2 + 1].p_lat, WayPoints[id2 + 1].p_long, WayPoints[id2 + 1].p_alt - WayPoints[0].p_alt);
			} else {
				mavlink_msg_mission_item_pack(127, 0, &msg2, ModelData[ModelActive].sysid, ModelData[ModelActive].compid, id, WayPoints[id2 + 1].frametype, type, 0, 1, WayPoints[id2 + 1].param1, WayPoints[id2 + 1].param2, WayPoints[id2 + 1].param3, WayPoints[id2 + 1].param4, WayPoints[id2 + 1].p_lat, WayPoints[id2 + 1].p_long, WayPoints[id2 + 1].p_alt);
			}
			mavlink_send_message(&msg2);

/*
mavlink_msg_mission_item_pack(system_id, component_id, &msg , packet1.target_system , packet1.target_component , packet1.seq , packet1.frame , packet1.command , packet1.current , packet1.autocontinue , packet1.param1 , packet1.param2 , packet1.param3 , packet1.param4 , packet1.x , packet1.y , packet1.z );
float param1; ///< PARAM1 / For NAV command MISSIONs: Radius in which the MISSION is accepted as reached, in meters
float param2; ///< PARAM2 / For NAV command MISSIONs: Time that the MAV should stay inside the PARAM1 radius before advancing, in milliseconds
float param3; ///< PARAM3 / For LOITER command MISSIONs: Orbit to circle around the MISSION, in meters. If positive the orbit direction should be clockwise, if negative the orbit direction should be counter-clockwise.
float param4; ///< PARAM4 / For NAV and LOITER command MISSIONs: Yaw orientation in degrees, [0..360] 0 = NORTH
float x; ///< PARAM5 / local: x position, global: latitude
float y; ///< PARAM6 / y position: global: longitude
float z; ///< PARAM7 / z position: global: altitude
uint16_t seq; ///< Sequence
uint16_t command; ///< The scheduled action for the MISSION. see MAV_CMD in common.xml MAVLink specs
uint8_t target_system; ///< System ID
uint8_t target_component; ///< Component ID
uint8_t frame; ///< The coordinate system of the MISSION. see MAV_FRAME in mavlink_types.h
uint8_t current; ///< false:0, true:1
uint8_t autocontinue; ///< autocontinue to next wp
*/

//			redraw_flag = 1;
			break;
		}
		case MAVLINK_MSG_ID_MISSION_ITEM: {
			mavlink_mission_item_t packet;
			mavlink_msg_mission_item_decode(msg, &packet);

			sprintf(sysmsg_str, "RECEIVED MISSION_ITEM: %i/%i: %f, %f, %f (%i)", packet.seq, mission_max, packet.x, packet.y, packet.z, packet.frame);
			SDL_Log("mavlink: %s\n", sysmsg_str);
			sys_message(sysmsg_str);
			sprintf(sysmsg_str, "	->: %f, %f, %f, %f", packet.param1, packet.param2, packet.param3, packet.param4);
			SDL_Log("mavlink: %s\n", sysmsg_str);
			sprintf(sysmsg_str, "	->: %i, %i, %i", packet.command, packet.current, packet.autocontinue);
			SDL_Log("mavlink: %s\n", sysmsg_str);



			if (packet.seq < mission_max - 1) {
				mavlink_msg_mission_request_pack(127, 0, &msg2, ModelData[ModelActive].sysid, ModelData[ModelActive].compid, packet.seq + 1);
				mavlink_send_message(&msg2);
			} else {
				mavlink_msg_mission_ack_pack(127, 0, &msg2, ModelData[ModelActive].sysid, ModelData[ModelActive].compid, 15);
				mavlink_send_message(&msg2);
			}

			if (ModelData[ModelActive].teletype == TELETYPE_MEGAPIRATE_NG || ModelData[ModelActive].teletype == TELETYPE_ARDUPILOT) {
				if (packet.seq > 0) {
					packet.seq = packet.seq - 1;
				} else {
					break;
				}
			}

			SDL_Log("mavlink: getting WP(%i): %f, %f\n", packet.seq, packet.x, packet.y);

			switch (packet.command) {
				case MAV_CMD_NAV_WAYPOINT: {
					strcpy(WayPoints[1 + packet.seq].command, "WAYPOINT");
					break;
				}
				case MAV_CMD_NAV_LOITER_UNLIM: {
					strcpy(WayPoints[1 + packet.seq].command, "LOITER_UNLIM");
					break;
				}
				case MAV_CMD_NAV_LOITER_TURNS: {
					strcpy(WayPoints[1 + packet.seq].command, "LOITER_TURNS");
					break;
				}
				case MAV_CMD_NAV_LOITER_TIME: {
					strcpy(WayPoints[1 + packet.seq].command, "LOITER_TIME");
					break;
				}
				case MAV_CMD_NAV_RETURN_TO_LAUNCH: {
					strcpy(WayPoints[1 + packet.seq].command, "RTL");
					break;
				}
				case MAV_CMD_NAV_LAND: {
					strcpy(WayPoints[1 + packet.seq].command, "LAND");
					break;
				}
				case MAV_CMD_NAV_TAKEOFF: {
					strcpy(WayPoints[1 + packet.seq].command, "TAKEOFF");
					break;
				}
				case MAV_CMD_DO_DIGICAM_CONTROL: {
					strcpy(WayPoints[1 + packet.seq].command, "SHUTTER");
					break;
				}
				case MAV_CMD_DO_SET_CAM_TRIGG_DIST: {
					strcpy(WayPoints[1 + packet.seq].command, "SHUTTER_INT");
					break;
				}
				case MAV_CMD_DO_SET_RELAY: {
					strcpy(WayPoints[1 + packet.seq].command, "RELAY");
					break;
				}
				case MAV_CMD_DO_REPEAT_RELAY: {
					strcpy(WayPoints[1 + packet.seq].command, "RELAY_REP");
					break;
				}
				case MAV_CMD_DO_SET_SERVO: {
					strcpy(WayPoints[1 + packet.seq].command, "SERVO");
					break;
				}
				case MAV_CMD_DO_REPEAT_SERVO: {
					strcpy(WayPoints[1 + packet.seq].command, "SERVO_REP");
					break;
				}
				case MAV_CMD_NAV_ROI: {
					strcpy(WayPoints[1 + packet.seq].command, "SET_ROI");
					break;
				}
				case 201: {
					strcpy(WayPoints[1 + packet.seq].command, "SET_ROI");
					break;
				}
				default: {
					sprintf(WayPoints[1 + packet.seq].command, "CMD:%i", packet.command);
					break;
				}
			}

			if (packet.x == 0.0) {
				packet.x = 0.00001;
			}
			if (packet.y == 0.0) {
				packet.y = 0.00001;
			}
			if (packet.z == 0.0) {
				packet.z = 0.00001;
			}


			if (ModelData[ModelActive].dronetype != MAV_TYPE_FIXED_WING && WayPoints[1 + packet.seq].frametype == MAV_FRAME_GLOBAL) {
				SDL_Log("mavlink: copter absolut alt workaround");
				packet.z += WayPoints[0].p_alt;
			}


			WayPoints[1 + packet.seq].p_lat = packet.x;
			WayPoints[1 + packet.seq].p_long = packet.y;
			WayPoints[1 + packet.seq].p_alt = packet.z;
			WayPoints[1 + packet.seq].param1 = packet.param1;
			WayPoints[1 + packet.seq].param2 = packet.param2;
			WayPoints[1 + packet.seq].param3 = packet.param3;
			WayPoints[1 + packet.seq].param4 = packet.param4;
			WayPoints[1 + packet.seq].frametype = packet.frame;
			sprintf(WayPoints[1 + packet.seq].name, "WP%i", packet.seq + 1);

			WayPoints[1 + packet.seq + 1].p_lat = 0.0;
			WayPoints[1 + packet.seq + 1].p_long = 0.0;
			WayPoints[1 + packet.seq + 1].p_alt = 0.0;
			WayPoints[1 + packet.seq + 1].param1 = 0.0;
			WayPoints[1 + packet.seq + 1].param2 = 0.0;
			WayPoints[1 + packet.seq + 1].param3 = 0.0;
			WayPoints[1 + packet.seq + 1].param4 = 0.0;
			WayPoints[1 + packet.seq + 1].name[0] = 0;
			WayPoints[1 + packet.seq + 1].frametype = 0;
			WayPoints[1 + packet.seq + 1].command[0] = 0;

//			redraw_flag = 1;
			break;
		}
		case MAVLINK_MSG_ID_MISSION_CURRENT: {
			mavlink_mission_current_t packet;
			mavlink_msg_mission_current_decode(msg, &packet);
//			SDL_Log("mavlink: ## Active_WP %f ##\n", packet.seq);
			uav_active_waypoint = (uint8_t)packet.seq;
			break;
		}
		case MAVLINK_MSG_ID_RAW_IMU: {
			mavlink_raw_imu_t packet;
			mavlink_msg_raw_imu_decode(msg, &packet);
			ModelData[ModelActive].acc_x = (float)packet.xacc / 1000.0;
			ModelData[ModelActive].acc_y = (float)packet.yacc / 1000.0;
			ModelData[ModelActive].acc_z = (float)packet.zacc / 1000.0;
			ModelData[ModelActive].gyro_x = (float)packet.zgyro;
			ModelData[ModelActive].gyro_y = (float)packet.zgyro;
			ModelData[ModelActive].gyro_z = (float)packet.zgyro;
//			redraw_flag = 1;
			break;
		}
		case MAVLINK_MSG_ID_NAV_CONTROLLER_OUTPUT: {
			mavlink_nav_controller_output_t packet;
			mavlink_msg_nav_controller_output_decode(msg, &packet);
			break;
		}
		case MAVLINK_MSG_ID_VFR_HUD: {
			mavlink_vfr_hud_t packet;
			mavlink_msg_vfr_hud_decode(msg, &packet);
			if (GPS_found == 0) {
				ModelData[ModelActive].p_alt = packet.alt;
			}
			break;
		}
		case MAVLINK_MSG_ID_RADIO_STATUS: {
			mavlink_radio_status_t packet;
			mavlink_msg_radio_status_decode(msg, &packet);
			ModelData[ModelActive].rssi_tx = packet.rssi / 2;
			ModelData[ModelActive].rssi_rx = packet.remrssi / 2;
			break;
		}
		case MAVLINK_MSG_ID_RADIO: {
			mavlink_radio_t packet;
			mavlink_msg_radio_decode(msg, &packet);
			ModelData[ModelActive].rssi_tx = packet.rssi / 2;
			ModelData[ModelActive].rssi_rx = packet.remrssi / 2;
			break;
		}
		case MAVLINK_MSG_ID_GLOBAL_POSITION_INT: {
			mavlink_global_position_int_t packet;
			mavlink_msg_global_position_int_decode(msg, &packet);
			if (GPS_found == 0) {
				if (packet.lat != 0 && packet.lon != 0) {
//					ModelData[ModelActive].p_lat = (float)packet.lat / 10000000.0;
//					ModelData[ModelActive].p_long = (float)packet.lon / 10000000.0;
//					ModelData[ModelActive].p_alt = (float)packet.alt / 1000.0;
//					ModelData[ModelActive].yaw = (float)packet.hdg / 100.0;
				}
			}
			break;
		}
		case MAVLINK_MSG_ID_SYSTEM_TIME: {
			mavlink_system_time_t packet;
			mavlink_msg_system_time_decode(msg, &packet);
			break;
		}
		case MAVLINK_MSG_ID_RC_CHANNELS: {
			mavlink_rc_channels_t packet;
			mavlink_msg_rc_channels_decode(msg, &packet);

			ModelData[ModelActive].radio_raw[0] = (int)packet.chan1_raw;
			ModelData[ModelActive].radio_raw[1] = (int)packet.chan2_raw;
			ModelData[ModelActive].radio_raw[2] = (int)packet.chan3_raw;
			ModelData[ModelActive].radio_raw[3] = (int)packet.chan4_raw;
			ModelData[ModelActive].radio_raw[4] = (int)packet.chan5_raw;
			ModelData[ModelActive].radio_raw[5] = (int)packet.chan6_raw;
			ModelData[ModelActive].radio_raw[6] = (int)packet.chan7_raw;
			ModelData[ModelActive].radio_raw[7] = (int)packet.chan8_raw;

			ModelData[ModelActive].radio[0] = (int)packet.chan1_raw / 5 - 300;
			ModelData[ModelActive].radio[1] = (int)packet.chan2_raw / 5 - 300;
			ModelData[ModelActive].radio[2] = (int)packet.chan3_raw / 5 - 300;
			ModelData[ModelActive].radio[3] = (int)packet.chan4_raw / 5 - 300;
			ModelData[ModelActive].radio[4] = (int)packet.chan5_raw / 5 - 300;
			ModelData[ModelActive].radio[5] = (int)packet.chan6_raw / 5 - 300;
			ModelData[ModelActive].radio[6] = (int)packet.chan7_raw / 5 - 300;
			ModelData[ModelActive].radio[7] = (int)packet.chan8_raw / 5 - 300;
			ModelData[ModelActive].radio[8] = (int)packet.chan9_raw / 5 - 300;
			ModelData[ModelActive].radio[9] = (int)packet.chan10_raw / 5 - 300;
			ModelData[ModelActive].radio[10] = (int)packet.chan11_raw / 5 - 300;
			ModelData[ModelActive].radio[11] = (int)packet.chan12_raw / 5 - 300;
			ModelData[ModelActive].radio[12] = (int)packet.chan13_raw / 5 - 300;
			ModelData[ModelActive].radio[13] = (int)packet.chan14_raw / 5 - 300;
			ModelData[ModelActive].radio[14] = (int)packet.chan15_raw / 5 - 300;
			ModelData[ModelActive].radio[15] = (int)packet.chan16_raw / 5 - 300;
			uint16_t n = 0;
			for (n = 1; n < 16; n++) {
				if (ModelData[ModelActive].radio[n] > 100) {
					ModelData[ModelActive].radio[n] = 100;
				} else if (ModelData[ModelActive].radio[n] < -100) {
					ModelData[ModelActive].radio[n] = -100;
				}
			}
			ModelData[ModelActive].rssi_rc_rx = (int)packet.rssi * 100 / 255;
			ModelData[ModelActive].chancount = (uint8_t)packet.chancount;
			break;
		}
		case MAVLINK_MSG_ID_AHRS: {
			mavlink_ahrs_t packet;
			mavlink_msg_ahrs_decode(msg, &packet);
			break;
		}
		case MAVLINK_MSG_ID_HWSTATUS: {
			mavlink_hwstatus_t packet;
			mavlink_msg_hwstatus_decode(msg, &packet);
			ModelData[ModelActive].fc_voltage1 = (float)packet.Vcc / 1000.0;
			ModelData[ModelActive].fc_i2c_errors = packet.I2Cerr;
			break;
		}
		case MAVLINK_MSG_ID_SCALED_IMU2: {
			mavlink_scaled_imu2_t packet;
			mavlink_msg_scaled_imu2_decode(msg, &packet);
			break;
		}
		case MAVLINK_MSG_ID_POWER_STATUS: {
			mavlink_power_status_t packet;
			mavlink_msg_power_status_decode(msg, &packet);
			ModelData[ModelActive].fc_voltage1 = (float)packet.Vcc / 1000.0;
			ModelData[ModelActive].fc_voltage2 = (float)packet.Vservo / 1000.0;
			ModelData[ModelActive].fc_status = packet.flags;
			break;
		}
		case MAVLINK_MSG_ID_MEMINFO: {
			mavlink_meminfo_t packet;
			mavlink_msg_meminfo_decode(msg, &packet);
			break;
		}
		case MAVLINK_MSG_ID_SENSOR_OFFSETS: {
			mavlink_sensor_offsets_t packet;
			mavlink_msg_sensor_offsets_decode(msg, &packet);
			break;
		}
		case MAVLINK_MSG_ID_AHRS2: {
			mavlink_ahrs2_t packet;
			mavlink_msg_ahrs2_decode(msg, &packet);
			ahrs2_found = 1;
			ModelData[ModelActive].roll = toDeg(packet.roll);
			ModelData[ModelActive].pitch = toDeg(packet.pitch);
			mavlink_update_yaw = 1;
			redraw_flag = 1;
			break;
		}
		case MAVLINK_MSG_ID_TERRAIN_REPORT: {
			mavlink_terrain_report_t packet;
			mavlink_msg_terrain_report_decode(msg, &packet);
/*
			SDL_Log("mavlink: ## MAVLINK_MSG_ID_TERRAIN_REPORT lat %i ##\n", packet.lat); //INT32_T
			SDL_Log("mavlink: ## MAVLINK_MSG_ID_TERRAIN_REPORT lon %i ##\n", packet.lon); //INT32_T
			SDL_Log("mavlink: ## MAVLINK_MSG_ID_TERRAIN_REPORT terrain_height %f ##\n", packet.terrain_height); //FLOAT
			SDL_Log("mavlink: ## MAVLINK_MSG_ID_TERRAIN_REPORT current_height %f ##\n", packet.current_height); //FLOAT
			SDL_Log("mavlink: ## MAVLINK_MSG_ID_TERRAIN_REPORT spacing %i ##\n", packet.spacing); //UINT16_T
			SDL_Log("mavlink: ## MAVLINK_MSG_ID_TERRAIN_REPORT pending %i ##\n", packet.pending); //UINT16_T
			SDL_Log("mavlink: ## MAVLINK_MSG_ID_TERRAIN_REPORT loaded %i ##\n", packet.loaded); //UINT16_T
*/
			break;
		}
		case MAVLINK_MSG_ID_TERRAIN_REQUEST: {
			mavlink_terrain_request_t packet;
			mavlink_msg_terrain_request_decode(msg, &packet);
/*
			SDL_Log("mavlink: ## MAVLINK_MSG_ID_TERRAIN_REQUEST mask %llu ##\n", packet.mask); //UINT64_T
			SDL_Log("mavlink: ## MAVLINK_MSG_ID_TERRAIN_REQUEST lat %i ##\n", packet.lat); //INT32_T
			SDL_Log("mavlink: ## MAVLINK_MSG_ID_TERRAIN_REQUEST lon %i ##\n", packet.lon); //INT32_T
			SDL_Log("mavlink: ## MAVLINK_MSG_ID_TERRAIN_REQUEST grid_spacing %i ##\n", packet.grid_spacing); //UINT16_T
*/
			break;
		}
		case MAVLINK_MSG_ID_MOUNT_STATUS: {
			mavlink_mount_status_t packet;
			mavlink_msg_mount_status_decode(msg, &packet);
			ModelData[ModelActive].mnt_pitch = (float)packet.pointing_a / 100.0;
			ModelData[ModelActive].mnt_roll = (float)packet.pointing_b / 100.0;
			ModelData[ModelActive].mnt_yaw = (float)packet.pointing_c / 100.0;
		}
		case MAVLINK_MSG_ID_WIND: {
			mavlink_wind_t packet;
			mavlink_msg_wind_decode(msg, &packet);
			break;
		}
		case MAVLINK_MSG_ID_SIMSTATE: {
			mavlink_simstate_t packet;
			mavlink_msg_simstate_decode(msg, &packet);
/*
			SDL_Log("mavlink: ## MAVLINK_MSG_ID_SIMSTATE roll %f ##\n", packet.roll); //FLOAT
			SDL_Log("mavlink: ## MAVLINK_MSG_ID_SIMSTATE pitch %f ##\n", packet.pitch); //FLOAT
			SDL_Log("mavlink: ## MAVLINK_MSG_ID_SIMSTATE yaw %f ##\n", packet.yaw); //FLOAT
			SDL_Log("mavlink: ## MAVLINK_MSG_ID_SIMSTATE xacc %f ##\n", packet.xacc); //FLOAT
			SDL_Log("mavlink: ## MAVLINK_MSG_ID_SIMSTATE yacc %f ##\n", packet.yacc); //FLOAT
			SDL_Log("mavlink: ## MAVLINK_MSG_ID_SIMSTATE zacc %f ##\n", packet.zacc); //FLOAT
			SDL_Log("mavlink: ## MAVLINK_MSG_ID_SIMSTATE xgyro %f ##\n", packet.xgyro); //FLOAT
			SDL_Log("mavlink: ## MAVLINK_MSG_ID_SIMSTATE ygyro %f ##\n", packet.ygyro); //FLOAT
			SDL_Log("mavlink: ## MAVLINK_MSG_ID_SIMSTATE zgyro %f ##\n", packet.zgyro); //FLOAT
			SDL_Log("mavlink: ## MAVLINK_MSG_ID_SIMSTATE lat %i ##\n", packet.lat); //INT32_T
			SDL_Log("mavlink: ## MAVLINK_MSG_ID_SIMSTATE lng %i ##\n", packet.lng); //INT32_T
*/
			break;
		}
		case MAVLINK_MSG_ID_LOG_ENTRY: {
			mavlink_log_entry_t packet;
			mavlink_msg_log_entry_decode(msg, &packet);
			SDL_Log("mavlink: ## MAVLINK_MSG_ID_LOG_ENTRY size %i ##\n", packet.size); //UINT32_T
			SDL_Log("mavlink: ## MAVLINK_MSG_ID_LOG_ENTRY id %i ##\n", packet.id); //UINT16_T
			SDL_Log("mavlink: ## MAVLINK_MSG_ID_LOG_ENTRY num_logs %i ##\n", packet.num_logs); //UINT16_T
			SDL_Log("mavlink: ## MAVLINK_MSG_ID_LOG_ENTRY last_log_num %i ##\n", packet.last_log_num); //UINT16_T

			loglist[mavlink_logs_total].id = packet.id;
			loglist[mavlink_logs_total].size = packet.size;
			mavlink_logs_total++;

			break;
		}
		case MAVLINK_MSG_ID_LOG_DATA: {
			mavlink_log_data_t packet;
			mavlink_msg_log_data_decode(msg, &packet);
			mavlink_logid = packet.id;
			mavlink_loggetsize = packet.ofs + packet.count;
			mavlink_logstat = mavlink_loggetsize * 100 / mavlink_logreqsize;
			mavlink_loghbeat = 100;
			if (mavlink_logstartstamp == 0) {
				mavlink_logstartstamp = SDL_GetTicks();
			}
			if (mavlink_loggetsize == mavlink_logreqsize) {
				mavlink_logstat = 100;
				mavlink_loghbeat = 255;
			}
//			SDL_Log("mavlink: ## MAVLINK_MSG_ID_LOG_DATA id:%i %i + %i (%i%%) ##\n", packet.id, packet.ofs, packet.count, mavlink_logstat);
			char tmp_str[1024];
			sprintf(tmp_str, "/tmp/mavlink_%i_%i.log", packet.id, mavlink_logreqsize);
			FILE *fdlog = fopen(tmp_str, "ab");
			fwrite(packet.data, packet.count, 1, fdlog);
			fclose(fdlog);
			break;
		}
		case MAVLINK_MSG_ID_COMMAND_ACK: {
			mavlink_command_ack_t packet;
			mavlink_msg_command_ack_decode(msg, &packet);
			SDL_Log("mavlink: ## MAVLINK_MSG_ID_COMMAND_ACK command %i ##\n", packet.command); //UINT16_T
			if (packet.result == MAV_RESULT_ACCEPTED) {
				SDL_Log("mavlink: ## MAVLINK_MSG_ID_COMMAND_ACK result %i: ACCEPTED ##\n", packet.result); //UINT8_T
			} else if (packet.result == MAV_RESULT_TEMPORARILY_REJECTED) {
				SDL_Log("mavlink: ## MAVLINK_MSG_ID_COMMAND_ACK result %i: TEMPORARILY_REJECTED ##\n", packet.result); //UINT8_T
			} else if (packet.result == MAV_RESULT_DENIED) {
				SDL_Log("mavlink: ## MAVLINK_MSG_ID_COMMAND_ACK result %i: DENIED ##\n", packet.result); //UINT8_T
			} else if (packet.result == MAV_RESULT_UNSUPPORTED) {
				SDL_Log("mavlink: ## MAVLINK_MSG_ID_COMMAND_ACK result %i: UNSUPPORTED ##\n", packet.result); //UINT8_T
			} else if (packet.result == MAV_RESULT_FAILED) {
				SDL_Log("mavlink: ## MAVLINK_MSG_ID_COMMAND_ACK result %i: FAILED ##\n", packet.result); //UINT8_T
			} else {
				SDL_Log("mavlink: ## MAVLINK_MSG_ID_COMMAND_ACK result %i: UNKNOWN ##\n", packet.result); //UINT8_T
			}
			break;
		}
		default: {
			SDL_Log("mavlink: ## UNSUPPORTED MSG_ID == %i (mavlink/get_case_by_file.sh %i) ##\n", msg->msgid, msg->msgid);
			break;
		}
	}
}

void mavlink_send_terrain_data (int32_t lat, int32_t lon, uint16_t grid_spacing, uint8_t gridbit, int16_t data[16]) {
	SDL_Log("mavlink: sending terrain_data\n");
	mavlink_message_t msg;
	mavlink_msg_terrain_data_pack(127, 0, &msg, lat, lon, grid_spacing, gridbit, data);
	mavlink_send_message(&msg);
}

void mavlink_read_waypoints (void) {
	SDL_Log("mavlink: reading Waypoints\n");
	mavlink_message_t msg;
	mavlink_msg_mission_request_list_pack(127, 0, &msg, ModelData[ModelActive].sysid, ModelData[ModelActive].compid);
	mavlink_send_message(&msg);
}

void mavlink_read_loglist (void) {
	int n = 0;
	SDL_Log("mavlink: reading loglist\n");
	mavlink_logs_total = 0;
	for (n = 0; n < 255; n++) {
		loglist[n].id = 0;
		loglist[n].size = 0;
	}
	mavlink_message_t msg;
	mavlink_msg_log_request_list_pack(127, 0, &msg, ModelData[ModelActive].sysid, ModelData[ModelActive].compid, 0, 0xffff);
	mavlink_send_message(&msg);
}

void mavlink_read_logfile (uint16_t id, uint32_t offset, uint32_t len) {
	char tmp_str[1024];
	sprintf(tmp_str, "/tmp/mavlink_%i_%i.log", id, len);
	FILE *fdlog = fopen(tmp_str, "w");
	fclose(fdlog);
	mavlink_loggetsize = 0;
	mavlink_logstartstamp = 0;
	mavlink_logreqsize = len;
	mavlink_loghbeat = 100;
	SDL_Log("mavlink: get logfile: %i (%ibytes)\n", id, len);
	mavlink_message_t msg;
	mavlink_msg_log_request_data_pack(127, 0, &msg, ModelData[ModelActive].sysid, ModelData[ModelActive].compid, id, offset, len);
	mavlink_send_message(&msg);
}

void mavlink_save_to_flash (void) {
	SDL_Log("mavlink: save values to flash\n");
	mavlink_message_t msg;
	mavlink_msg_command_long_pack(127, 0, &msg, ModelData[ModelActive].sysid, ModelData[ModelActive].compid, MAV_CMD_PREFLIGHT_STORAGE, 0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
	mavlink_send_message(&msg);
}

void mavlink_load_from_flash (void) {
	SDL_Log("mavlink: load values from flash\n");
	mavlink_message_t msg;
	mavlink_msg_command_long_pack(127, 0, &msg, ModelData[ModelActive].sysid, ModelData[ModelActive].compid, MAV_CMD_PREFLIGHT_STORAGE, 0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
	mavlink_send_message(&msg);
}

void mavlink_send_cmd_calibration (void) {
	SDL_Log("mavlink: send cmd: Calibration\n");
	mavlink_message_t msg;
	mavlink_msg_command_long_pack(127, 0, &msg, ModelData[ModelActive].sysid, ModelData[ModelActive].compid, MAV_CMD_PREFLIGHT_CALIBRATION, 0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0);
	mavlink_send_message(&msg);
}

void mavlink_send_cmd_calibration_ack (void) {
	SDL_Log("mavlink: send cmd: Calibration ACK\n");
	mavlink_message_t msg;
	mavlink_msg_command_ack_pack(127, 0, &msg, MAV_CMD_PREFLIGHT_CALIBRATION, MAV_CMD_ACK_OK);
	mavlink_send_message(&msg);
}

void mavlink_send_cmd_rtl (void) {
	SDL_Log("mavlink: send cmd: RTL\n");
	mavlink_message_t msg;
	mavlink_msg_command_long_pack(127, 0, &msg, ModelData[ModelActive].sysid, ModelData[ModelActive].compid, MAV_CMD_NAV_RETURN_TO_LAUNCH, 0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
	mavlink_send_message(&msg);
}

void mavlink_send_cmd_land (void) {
	SDL_Log("mavlink: send cmd: RTL\n");
	mavlink_message_t msg;
	mavlink_msg_command_long_pack(127, 0, &msg, ModelData[ModelActive].sysid, ModelData[ModelActive].compid, MAV_CMD_NAV_LAND, 0, 0.0, 0.0, 0.0, 0.0, ModelData[ModelActive].p_lat, ModelData[ModelActive].p_long, ModelData[ModelActive].p_alt);
	mavlink_send_message(&msg);
}

void mavlink_send_cmd_takeoff (void) {
	SDL_Log("mavlink: send cmd: RTL\n");
	mavlink_message_t msg;
	mavlink_msg_command_long_pack(127, 0, &msg, ModelData[ModelActive].sysid, ModelData[ModelActive].compid, MAV_CMD_NAV_TAKEOFF, 0, 0.0, 0.0, 0.0, 0.0, ModelData[ModelActive].p_lat, ModelData[ModelActive].p_long, ModelData[ModelActive].p_alt);
	mavlink_send_message(&msg);
}

void mavlink_send_cmd_mission (void) {
	SDL_Log("mavlink: send cmd: MISSION\n");
	mavlink_message_t msg;
	mavlink_msg_command_long_pack(127, 0, &msg, ModelData[ModelActive].sysid, ModelData[ModelActive].compid, MAV_CMD_MISSION_START, 0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
	mavlink_send_message(&msg);
}

void mavlink_send_cmd_arm (uint8_t mode) {
	SDL_Log("mavlink: send cmd: ARM: %i\n", mode);
	mavlink_message_t msg;
	mavlink_msg_command_long_pack(127, 0, &msg, ModelData[ModelActive].sysid, ModelData[ModelActive].compid, MAV_CMD_COMPONENT_ARM_DISARM, 0, (float)mode, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
	mavlink_send_message(&msg);
}

void mavlink_send_cmd_mode (uint8_t mode) {
	SDL_Log("mavlink: send cmd: MODE: %i\n", mode);
	mavlink_message_t msg;
	mavlink_msg_command_long_pack(127, 0, &msg, ModelData[ModelActive].sysid, ModelData[ModelActive].compid, MAV_CMD_DO_SET_MODE, 0, (float)mode, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
	mavlink_send_message(&msg);
}

void mavlink_send_cmd_guided (void) {
	SDL_Log("mavlink: send cmd: GUIDED\n");
	mavlink_message_t msg;
	mavlink_msg_command_long_pack(127, 0, &msg, ModelData[ModelActive].sysid, ModelData[ModelActive].compid, MAV_CMD_DO_SET_MODE, 0, MAV_MODE_GUIDED_ARMED, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
	mavlink_send_message(&msg);
}

void mavlink_send_cmd_loiter (void) {
	SDL_Log("mavlink: send cmd: LOITER\n");
	mavlink_message_t msg;
	mavlink_msg_command_long_pack(127, 0, &msg, ModelData[ModelActive].sysid, ModelData[ModelActive].compid, MAV_CMD_NAV_LOITER_UNLIM, 0, 0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
	mavlink_send_message(&msg);
}

void mavlink_send_cmd_follow (float p_lat, float p_long, float p_alt, float radius) {
//	SDL_Log("mavlink: send cmd: FOLLOW\n");
	mavlink_message_t msg;
	mavlink_msg_mission_item_pack(127, 0, &msg, ModelData[ModelActive].sysid, ModelData[ModelActive].compid, 0, MAV_FRAME_GLOBAL_RELATIVE_ALT, MAV_CMD_NAV_WAYPOINT, 2, 0, 0.0, radius, 0.0, 0.0, p_lat, p_long, p_alt);
	mavlink_send_message(&msg);
}

void mavlink_send_waypoints (void) {
	mavlink_message_t msg;
	mavlink_msg_mission_clear_all_pack(127, 0, &msg, ModelData[ModelActive].sysid, ModelData[ModelActive].compid);
	mavlink_send_message(&msg);
	usleep(100000);
	uint16_t n = 0;
	for (n = 1; n < MAX_WAYPOINTS; n++) {
		if (WayPoints[n].p_lat == 0.0) {
			break;
		}
	}
	if (ModelData[ModelActive].teletype == TELETYPE_MEGAPIRATE_NG || ModelData[ModelActive].teletype == TELETYPE_ARDUPILOT) {
		SDL_Log("mavlink: WORKAROUND: MEGAPIRATE_NG: fake one WP\n");
		n++;
	}
	SDL_Log("mavlink: sending Waypoints (%i)\n", n - 1);
	mavlink_msg_mission_count_pack(127, 0, &msg, ModelData[ModelActive].sysid, ModelData[ModelActive].compid, n - 1);
	mavlink_send_message(&msg);
}

void mavlink_send_message (mavlink_message_t* msg) {
	uint8_t buf[MAVLINK_MAX_PACKET_LEN];
//	SDL_Log("mavlink: send_msg...\n");
	uint16_t len = mavlink_msg_to_send_buffer(buf, msg);
	uint16_t i = 0;
	for(i = 0; i < len; i++) {
		uint8_t c = buf[i];
		serial_write(serial_fd_mavlink, &c, 1);
	}
#ifndef WINDOWS
	if (mavlink_udp_active == 1) {
		if (sendto(s, buf, len, 0, (struct sockaddr *)&si_other, slen) == -1) {
			SDL_Log("mavlink: error: sendto udp()\n");
		}
	}
	if (mavlink_tcp_active == 1) {
		mavlink_tcp_send(buf, len);
	}
#endif
}

uint8_t mavlink_connection_status (void) {
	if (serial_fd_mavlink == -1) {
		return 0;
	}
	return last_connection;
}

void mavlink_update (void) {
	if (serial_fd_mavlink == -1) {
		return;
	}
	uint8_t c = 0;
	uint16_t n = 0;
	mavlink_message_t msg;
	mavlink_status_t status;
	while ((res = serial_read(serial_fd_mavlink, serial_buf, 250)) > 0) {
		last_connection = time(0);
		for (n = 0; n < res; n++) {
			c = serial_buf[n];
			if(mavlink_parse_char(0, c, &msg, &status)) {
				mavlink_handleMessage(&msg);
			}
		}
	}
}

void mavlink_param_get_id (uint16_t id) {
	SDL_Log("mavlink: get id: %i\n", id);
	mavlink_message_t msg;
	mavlink_msg_param_request_read_pack(127, 0, &msg, ModelData[ModelActive].sysid, ModelData[ModelActive].compid, NULL, id);
	mavlink_send_message(&msg);
}

void mavlink_start_feeds (void) {
	mavlink_message_t msg;
	mavlink_timeout = 0;
	param_complete = 0;

//	mavlink_maxparam = 1;

	SDL_Log("mavlink: starting feeds!\n");
	mavlink_msg_param_request_list_pack(127, 0, &msg, ModelData[ModelActive].sysid, ModelData[ModelActive].compid);
	mavlink_send_message(&msg);
	SDL_Delay(30);

	mavlink_msg_request_data_stream_pack(127, 0, &msg, ModelData[ModelActive].sysid, ModelData[ModelActive].compid, MAV_DATA_STREAM_RAW_SENSORS, MAV_DATA_STREAM_RAW_SENSORS_RATE, MAV_DATA_STREAM_RAW_SENSORS_ACTIVE);
	mavlink_send_message(&msg);
	SDL_Delay(30);

	mavlink_msg_request_data_stream_pack(127, 0, &msg, ModelData[ModelActive].sysid, ModelData[ModelActive].compid, MAV_DATA_STREAM_RC_CHANNELS, MAV_DATA_STREAM_RC_CHANNELS_RATE, MAV_DATA_STREAM_RC_CHANNELS_ACTIVE);
	mavlink_send_message(&msg);
	SDL_Delay(30);

	if (ModelData[ModelActive].teletype == TELETYPE_MEGAPIRATE_NG || ModelData[ModelActive].teletype == TELETYPE_ARDUPILOT || ModelData[ModelActive].teletype == TELETYPE_HARAKIRIML) {
		mavlink_msg_request_data_stream_pack(127, 0, &msg, ModelData[ModelActive].sysid, ModelData[ModelActive].compid, MAV_DATA_STREAM_EXTENDED_STATUS, MAV_DATA_STREAM_EXTENDED_STATUS_RATE, MAV_DATA_STREAM_EXTENDED_STATUS_ACTIVE);
		mavlink_send_message(&msg);
		SDL_Delay(30);

		mavlink_msg_request_data_stream_pack(127, 0, &msg, ModelData[ModelActive].sysid, ModelData[ModelActive].compid, MAV_DATA_STREAM_RAW_CONTROLLER, MAV_DATA_STREAM_RAW_CONTROLLER_RATE, MAV_DATA_STREAM_RAW_CONTROLLER_ACTIVE);
		mavlink_send_message(&msg);
		SDL_Delay(30);

		mavlink_msg_request_data_stream_pack(127, 0, &msg, ModelData[ModelActive].sysid, ModelData[ModelActive].compid, MAV_DATA_STREAM_POSITION, MAV_DATA_STREAM_POSITION_RATE, MAV_DATA_STREAM_POSITION_ACTIVE);
		mavlink_send_message(&msg);
		SDL_Delay(30);

		mavlink_msg_request_data_stream_pack(127, 0, &msg, ModelData[ModelActive].sysid, ModelData[ModelActive].compid, MAV_DATA_STREAM_EXTRA1, MAV_DATA_STREAM_EXTRA1_RATE, MAV_DATA_STREAM_EXTRA1_ACTIVE);
		mavlink_send_message(&msg);
		SDL_Delay(30);

		mavlink_msg_request_data_stream_pack(127, 0, &msg, ModelData[ModelActive].sysid, ModelData[ModelActive].compid, MAV_DATA_STREAM_EXTRA2, MAV_DATA_STREAM_EXTRA2_RATE, MAV_DATA_STREAM_EXTRA2_ACTIVE);
		mavlink_send_message(&msg);
		SDL_Delay(30);

		mavlink_msg_request_data_stream_pack(127, 0, &msg, ModelData[ModelActive].sysid, ModelData[ModelActive].compid, MAV_DATA_STREAM_EXTRA3, MAV_DATA_STREAM_EXTRA3_RATE, MAV_DATA_STREAM_EXTRA3_ACTIVE);
		mavlink_send_message(&msg);
		SDL_Delay(30);
	}
}

void mavlink_parseParams1 (xmlDocPtr doc, xmlNodePtr cur, char *name) { 
	int n = 0;
	int n2 = 0;
	int n3 = 0;
	int n4 = 0;
	char tmp_str3[1024];
	for (n = 0; n < MAVLINK_PARAMETER_MAX; n++) {
		if (strcmp(MavLinkVars[n].name, name) == 0) {
			break;
		}
	}
	if (n == MAVLINK_PARAMETER_MAX) {
		return;
	}
	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		if ((!xmlStrcasecmp(cur->name, (const xmlChar *)"Range"))) {
			float min = 0.0;
			float max = 0.0;
			xmlChar *key;
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL) {
				sscanf((char *)key, "%f %f", &min, &max);
				MavLinkVars[n].min = min;
				MavLinkVars[n].max = max;
				if (MavLinkVars[n].min > MavLinkVars[n].value) {
					MavLinkVars[n].min = MavLinkVars[n].value;
				}
				if (MavLinkVars[n].max < MavLinkVars[n].value) {
					MavLinkVars[n].max = MavLinkVars[n].value;
				}
				xmlFree(key);
			}
		} else if ((!xmlStrcasecmp(cur->name, (const xmlChar *)"Description"))) {
			xmlChar *key;
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL) {
				strncpy(MavLinkVars[n].desc, (char *)key, 1023);
				xmlFree(key);
			}
		} else if ((!xmlStrcasecmp(cur->name, (const xmlChar *)"Group"))) {
			xmlChar *key;
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL) {
				strncpy(MavLinkVars[n].group, (char *)key, 20);
				xmlFree(key);
			}
		} else if ((!xmlStrcasecmp(cur->name, (const xmlChar *)"Values"))) {
			xmlChar *key;
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL) {
				strncpy(MavLinkVars[n].values, (char *)key, 1023);
				if (MavLinkVars[n].values[0] != 0) {
					MavLinkVars[n].min = atof(MavLinkVars[n].values);
					for (n2 = 0; n2 < strlen(MavLinkVars[n].values); n2++) {
						if (MavLinkVars[n].values[n2] == ',') {
							strncpy(tmp_str3, MavLinkVars[n].values + n3, 1023);
							for (n4 = 0; n4 < strlen(tmp_str3); n4++) {
								if (tmp_str3[n4] == ',') {
									tmp_str3[n4] = 0;
									break;
								}
							}
							n3 = n2 + 1;
						}
					}
					strncpy(tmp_str3, MavLinkVars[n].values + n3, 1023);
					MavLinkVars[n].max = atof(tmp_str3);
				}
				xmlFree(key);
			}
		} else if ((!xmlStrcasecmp(cur->name, (const xmlChar *)"Bits"))) {
			xmlChar *key;
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL) {
				strncpy(MavLinkVars[n].bits, (char *)key, 1023);
				if (MavLinkVars[n].bits[0] != 0) {
					MavLinkVars[n].min = 0;
					for (n2 = 0; n2 < strlen(MavLinkVars[n].bits); n2++) {
						if (MavLinkVars[n].bits[n2] == ',') {
							strncpy(tmp_str3, MavLinkVars[n].bits + n3, 1023);
							for (n4 = 0; n4 < strlen(tmp_str3); n4++) {
								if (tmp_str3[n4] == ',') {
									tmp_str3[n4] = 0;
									break;
								}
							}
							n3 = n2 + 1;
						}
					}
					strncpy(tmp_str3, MavLinkVars[n].bits + n3, 1023);
					MavLinkVars[n].max = (float)((1<<atoi(tmp_str3)) * (1<<atoi(tmp_str3)) - 1);
				}
				xmlFree(key);
			}
		} else if ((!xmlStrcasecmp(cur->name, (const xmlChar *)"DisplayName"))) {
			xmlChar *key;
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL) {
				strncpy(MavLinkVars[n].display, (char *)key, 100);
				xmlFree(key);
			}
		}
		cur = cur->next;
	}
	return;
}

void mavlink_parseParamsGetAttr (xmlDocPtr doc, xmlNodePtr cur, char *attrName, char *attrValue) {
	xmlAttrPtr attribute = NULL;
	for (attribute = cur->properties; attribute != NULL; attribute = attribute->next){
		if (!(xmlStrcasecmp(attribute->name, (const xmlChar *)attrName))) {
			xmlChar *content = xmlNodeListGetString(doc, attribute->children, 1);
			strcpy(attrValue, (char *)content);
			xmlFree(content);
			return;
		}
	}
	attrValue[0] = 0;
}

size_t trimline (char *out, size_t len, const char *str) {
	if(len == 0) {
		return 0;
	}
	const char *end;
	size_t out_size;
	while (*str == ' ' || *str == '\r' || *str == '\n') {
		str++;
	}
	if (*str == 0) {
		*out = 0;
		return 1;
	}
	end = str + strlen(str) - 1;
	while(end > str && (*end == ' ' || *end == '\r' || *end == '\n')) {
		end--;
	}
	end++;
	out_size = (end - str) < (int)len - 1 ? (end - str) : len - 1;
	memcpy(out, str, out_size);
	out[out_size] = 0;
	return out_size;
}

void mavlink_parseParams1New (xmlDocPtr doc, xmlNodePtr cur, char *name, char *disp, char *desc) { 
	int n = 0;
	for (n = 0; n < MAVLINK_PARAMETER_MAX; n++) {
		if (strcmp(MavLinkVars[n].name, name) == 0) {
			break;
		}
	}
	if (n == MAVLINK_PARAMETER_MAX) {
		return;
	}
	strncpy(MavLinkVars[n].desc, (char *)desc, 1023);
	strncpy(MavLinkVars[n].display, (char *)disp, 100);
	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		char attrValue[1024];
		mavlink_parseParamsGetAttr(doc, cur, "Name", attrValue);
		if ((!xmlStrcasecmp((const xmlChar *)attrValue, (const xmlChar *)"Range"))) {
			float min = 0.0;
			float max = 0.0;
			xmlChar *key;
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL) {
				sscanf((char *)key, "%f %f", &min, &max);
				MavLinkVars[n].min = min;
				MavLinkVars[n].max = max;
				if (MavLinkVars[n].min > MavLinkVars[n].value) {
					MavLinkVars[n].min = MavLinkVars[n].value;
				}
				if (MavLinkVars[n].max < MavLinkVars[n].value) {
					MavLinkVars[n].max = MavLinkVars[n].value;
				}
				xmlFree(key);
			}
		} else if ((!xmlStrcasecmp((const xmlChar *)attrValue, (const xmlChar *)"Group"))) {
			xmlChar *key;
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL) {
				strncpy(MavLinkVars[n].group, (char *)key, 20);
				xmlFree(key);
			}
		} else if ((!xmlStrcasecmp(cur->name, (const xmlChar *)"Values"))) {
			float last_val = 0.0;
			MavLinkVars[n].values[0] = 0;
			xmlNodePtr cur2 = cur->xmlChildrenNode;
			while (cur2 != NULL) {
				if (!(xmlStrcasecmp(cur2->name, (const xmlChar *)"Value"))) {
					xmlChar *key;
					key = xmlNodeListGetString(doc, cur2->xmlChildrenNode, 1);
					if (key != NULL) {
						char Value[1024];
						char Value2[1024];
						mavlink_parseParamsGetAttr(doc, cur2, "Code", Value);
						trimline(Value2, 1024, Value);
						last_val = atof(Value2);
						if (MavLinkVars[n].values[0] == 0) {
							MavLinkVars[n].min = last_val;
						} else {
							strcat(MavLinkVars[n].values, ",");
						}
						strcat(MavLinkVars[n].values, Value2);
						strcat(MavLinkVars[n].values, ":");
						strcat(MavLinkVars[n].values, (char *)key);
						xmlFree(key);
					}
				}
				cur2 = cur2->next;
			}
			MavLinkVars[n].max = last_val;
		}
		cur = cur->next;
	}
	return;
}

void mavlink_parseParamsNew (xmlDocPtr doc, xmlNodePtr cur) { 
	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		if (!(xmlStrcasecmp(cur->name, (const xmlChar *)"param"))) {
			char attrValue[1024];
			char Desc[1024];
			char Disp[1024];
			mavlink_parseParamsGetAttr(doc, cur, "Name", attrValue);
			mavlink_parseParamsGetAttr(doc, cur, "Documentation", Desc);
			mavlink_parseParamsGetAttr(doc, cur, "humanName", Disp);
			if (strncmp(attrValue, "ArduCopter:", 11) == 0) {
				mavlink_parseParams1New(doc, cur, attrValue + 11, Disp, Desc);
			}
		}
		cur = cur->next;
	}
	return;
}

void mavlink_parseParams (xmlDocPtr doc, xmlNodePtr cur) { 
	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		if (!(xmlStrcasecmp(cur->name, (const xmlChar *)"parameters"))) {
			mavlink_parseParamsNew(doc, cur);
		} else if ((xmlStrcasecmp(cur->name, (const xmlChar *)"text"))) {
			mavlink_parseParams1(doc, cur, (char *)cur->name);
		}
		cur = cur->next;
	}
	return;
}

static void mavlink_parseDoc (char *docname) {
	xmlDocPtr doc;
	xmlNodePtr cur;
	if (strncmp(docname, "./", 2) == 0) {
		docname += 2;
	}
	char *buffer = NULL;
	int len = 0;
	SDL_RWops *ops_file = SDL_RWFromFile(docname, "r");
	if (ops_file == NULL) {
		SDL_Log("map: Document open failed: %s\n", docname);
		return;
	}
	len = SDL_RWseek(ops_file, 0, SEEK_END);
	SDL_RWseek(ops_file, 0, SEEK_SET);
	buffer = malloc(len);
	SDL_RWread(ops_file, buffer, 1, len);
	doc = xmlParseMemory(buffer, len);
	SDL_RWclose(ops_file);
	free(buffer);
	if (doc == NULL) {
		SDL_Log("mavlink: Document parsing failed: %s\n", docname);
		return;
	}
	cur = xmlDocGetRootElement(doc);
	if (cur == NULL) {
		xmlFreeDoc(doc);
		SDL_Log("mavlink: Document is Empty!!!\n");
		return;
	}
	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		if ((xmlStrcasecmp(cur->name, (const xmlChar *)"vehicles")) == 0) {
			mavlink_parseParams(doc, cur);
		} else if ((xmlStrcasecmp(cur->name, (const xmlChar *)"ArduCopter2")) == 0) {
			mavlink_parseParams(doc, cur);
		}
		cur = cur->next;
	}
	xmlFreeDoc(doc);
	return;
}

void mavlink_param_xml_meta_load (void) {
	char filename[1024];
	sprintf(filename, "%s/mavlink/ParameterMetaData-%s.xml", get_datadirectory(), teletypes[ModelData[ModelActive].teletype]);
	if (file_exists(filename) != 0) {
		mavlink_parseDoc(filename);
		return;
	}
	sprintf(filename, "%s/mavlink/ParameterMetaData-%s.xml", BASE_DIR, teletypes[ModelData[ModelActive].teletype]);
	if (file_exists(filename) != 0) {
		mavlink_parseDoc(filename);
		return;
	}
}

static void model_parseMavlinkParam (xmlDocPtr doc, xmlNodePtr cur, uint16_t param) { 
	xmlChar *key;
	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		if ((!xmlStrcasecmp(cur->name, (const xmlChar *)"name"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if ((char *)key != NULL) {
				strncpy(MavLinkVars[param].name, (char *)key, 20);
			}
			xmlFree(key);
		} else if ((!xmlStrcasecmp(cur->name, (const xmlChar *)"value"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if ((char *)key != NULL) {
				MavLinkVars[param].value = atof((char *)key);
				MavLinkVars[param].onload = atof((char *)key);
			}
			xmlFree(key);
		}
		cur = cur->next;
	}
	return;
}

void mavlink_xml_load (xmlDocPtr doc, xmlNodePtr cur) { 
	uint16_t param = 0;
	for (param = 0; param < MAVLINK_PARAMETER_MAX; param++) {
		MavLinkVars[param].name[0] = 0;
		MavLinkVars[param].group[0] = 0;
		MavLinkVars[param].value = 0.0;
		MavLinkVars[param].onload = 0.0;
	}
	param = 0;
	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		if ((!xmlStrcasecmp(cur->name, (const xmlChar *)"param"))) {
			model_parseMavlinkParam (doc, cur, param++);
		}
		cur = cur->next;
	}
	return;
}

static void mavlink_html_page (char *content, char *sub) {
	char tmp_str[512];
	char tmp_str2[512];
	uint16_t n = 0;
	uint16_t n2 = 0;
	content[0] = 0;
	webserv_html_head(content, "MAVLINK");
	strcat(content, "<SCRIPT>\n");
	strcat(content, "function check_option(name) {\n");
	strcat(content, "	var value = document.getElementById(name).options[document.getElementById(name).selectedIndex].value;\n");
	strcat(content, "	xmlHttp = new XMLHttpRequest();\n");
	strcat(content, "	xmlHttp.open(\"GET\", \"/mavlink_value_set?\" + name + \"=\" + value, true);\n");
	strcat(content, "	xmlHttp.send(null);\n");
	strcat(content, "}\n");
	strcat(content, "function check_value(name) {\n");
	strcat(content, "	var value = document.getElementById(name).value;\n");
	strcat(content, "	xmlHttp = new XMLHttpRequest();\n");
	strcat(content, "	xmlHttp.open(\"GET\", \"/mavlink_value_set?\" + name + \"=\" + value, true);\n");
	strcat(content, "	xmlHttp.send(null);\n");
	strcat(content, "}\n");
	strcat(content, "</SCRIPT>\n");
	webserv_html_start(content, 0);
	strcat(content, "<TABLE class=\"main\">\n");
	strcat(content, "<TR class=\"main\"><TD width=\"160px\" valign=\"top\">\n");
	strcat(content, "<TABLE width=\"100%\">\n");
	strcat(content, "<TR class=\"thead\"><TH>MODE</TH></TR>\n");
	strcat(content, "<TR class=\"first\"><TD><A href=\"/mavlink.html\">ALL</A></TD></TR>");
	uint8_t flag = 0;
	char mavlink_subs[512][128];
	for (n2 = 0; n2 < MAVLINK_PARAMETER_MAX; n2++) {
		mavlink_subs[n2][0] = 0;
	}
	for (n = 0; n < MAVLINK_PARAMETER_MAX; n++) {
		if (MavLinkVars[n].name[0] != 0) {
			strcpy(tmp_str, MavLinkVars[n].name);
			if (MavLinkVars[n].group[0] == 0) {
				for (n2 = 0; n2 < strlen(tmp_str) && n2 < 6; n2++) {
					if (tmp_str[n2] == '_') {
						break;
					}
				}
				strncpy(MavLinkVars[n].group, tmp_str, 17);
			}
			flag = 0;
			for (n2 = 0; n2 < MAVLINK_PARAMETER_MAX; n2++) {
				if (strcmp(mavlink_subs[n2], MavLinkVars[n].group) == 0) {
					flag = 1;
					break;
				}
			}
			if (flag == 0) {
				if (strcmp(MavLinkVars[n].group, sub) == 0) {
					sprintf(tmp_str2, "<TR class=\"sec\"><TD><A href=\"/mavlink.html?%s\">%s</A></TD></TR>", MavLinkVars[n].group, MavLinkVars[n].group);
				} else {
					sprintf(tmp_str2, "<TR class=\"first\"><TD><A href=\"/mavlink.html?%s\">%s</A></TD></TR>", MavLinkVars[n].group, MavLinkVars[n].group);
				}
				strcat(content, tmp_str2);
				for (n2 = 0; n2 < MAVLINK_PARAMETER_MAX; n2++) {
					if (mavlink_subs[n2][0] == 0) {
						strcpy(mavlink_subs[n2], MavLinkVars[n].group);
						break;
					}
				}
			}
		}
	}
	strcat(content, "</TABLE><BR><BR><BR>\n");
	strcat(content, "</TD><TD valign=\"top\" width=\"20px\">&nbsp;</TD><TD valign=\"top\">\n");
	strcat(content, "<TABLE width=\"100%\" border=\"0\">\n");
	strcat(content, "<TR class=\"thead\"><TH>NAME</TH><TH>VALUE</TH><TH>DESCRIPTION</TH><TH>MIN</TH><TH>MAX</TH><TH>ONLOAD</TH></TR>\n");
	int lc = 0;
	for (n = 0; n < MAVLINK_PARAMETER_MAX; n++) {
		if (MavLinkVars[n].name[0] != 0 && (sub[0] == 0 || strncmp(MavLinkVars[n].name, sub, strlen(sub)) == 0 || strcmp(MavLinkVars[n].group, sub) == 0)) {
			lc = 1 - lc;
			if (lc == 0) {
				strcat(content, "<TR class=\"first\">");
			} else {
				strcat(content, "<TR class=\"sec\">");
			}
			sprintf(tmp_str, "<TD>%s (%s)</TD>\n", MavLinkVars[n].name, MavLinkVars[n].display);
			strcat(content, tmp_str);
			if (MavLinkVars[n].values[0] != 0) {
				int n2 = 0;
				sprintf(tmp_str, "<TD><SELECT class=\"form-input\" onchange=\"check_option('%s');\" id=\"%s\">\n", MavLinkVars[n].name, MavLinkVars[n].name);
				strcat(content, tmp_str);
				tmp_str2[0] = 0;
				for (n2 = (int)MavLinkVars[n].min; n2 <= (int)MavLinkVars[n].max; n2++) {
					tmp_str2[0] = 0;
					mavlink_meta_get_option(n2, MavLinkVars[n].name, tmp_str2);
					if (tmp_str2[0] != 0) {
						if (n2 == (int)MavLinkVars[n].value) {
							sprintf(tmp_str, "<OPTION value=\"%i\" selected>%s</OPTION>\n", n2, tmp_str2);
						} else {
							sprintf(tmp_str, "<OPTION value=\"%i\">%s</OPTION>\n", n2, tmp_str2);
						}
						strcat(content, tmp_str);
					}
				}
				strcat(content, "</SELECT></TD>");
			} else if (MavLinkVars[n].bits[0] != 0) {
				sprintf(tmp_str, "<TD>\n");
				strcat(content, tmp_str);
				int n2 = 0;
				strcat(content, "<SCRIPT>\n");
				sprintf(tmp_str, "function check_%s() {\n", MavLinkVars[n].name);
				strcat(content, tmp_str);
				strcat(content, "	var value = 0;\n");
				tmp_str2[0] = 0;
				for (n2 = (int)MavLinkVars[n].min; n2 <= (int)MavLinkVars[n].max; n2++) {
					tmp_str2[0] = 0;
					mavlink_meta_get_bits(n2, MavLinkVars[n].name, tmp_str2);
					if (tmp_str2[0] != 0) {
						sprintf(tmp_str, "	if (document.getElementsByName(\"%s-%s\")[0].checked) {\n", MavLinkVars[n].name, tmp_str2);
						strcat(content, tmp_str);
						sprintf(tmp_str, "		value |= (1<<%i);\n", n2);
						strcat(content, tmp_str);
						strcat(content, "	}\n");
					}
				}
				strcat(content, "	xmlHttp = new XMLHttpRequest();\n");
				sprintf(tmp_str, "	xmlHttp.open(\"GET\", \"/mavlink_value_set?%s=\" + value, true);\n", MavLinkVars[n].name);
				strcat(content, tmp_str);
				strcat(content, "	xmlHttp.send(null);\n");
				strcat(content, "}\n");
				strcat(content, "</SCRIPT>\n");
				tmp_str2[0] = 0;
				for (n2 = (int)MavLinkVars[n].min; n2 <= (int)MavLinkVars[n].max; n2++) {
					tmp_str2[0] = 0;
					mavlink_meta_get_bits(n2, MavLinkVars[n].name, tmp_str2);
					if (tmp_str2[0] != 0) {
						if ((int)MavLinkVars[n].value & (1<<n2)) {
							sprintf(tmp_str, "<NOBR><INPUT class=\"form-input\" onchange=\"check_%s();\" type=\"checkbox\" name=\"%s-%s\" value=\"%i\" checked>%s</NOBR>\n", MavLinkVars[n].name, MavLinkVars[n].name, tmp_str2, n2, tmp_str2);
						} else {
							sprintf(tmp_str, "<NOBR><INPUT class=\"form-input\" onchange=\"check_%s();\" type=\"checkbox\" name=\"%s-%s\" value=\"%i\">%s</NOBR>\n", MavLinkVars[n].name, MavLinkVars[n].name, tmp_str2, n2, tmp_str2);
						}
						strcat(content, tmp_str);
					}
				}
				strcat(content, "</TD>");
			} else {
				sprintf(tmp_str, "<TD><INPUT class=\"form-input\" onchange=\"check_value('%s');\" id=\"%s\" value=\"%f\" type=\"text\"></TD>\n", MavLinkVars[n].name, MavLinkVars[n].name, MavLinkVars[n].value);
				strcat(content, tmp_str);
			}
			sprintf(tmp_str, "<TD>%s&nbsp;</TD><TD>%0.4f</TD><TD>%0.4f</TD><TD>%0.4f</TD></TR>\n", MavLinkVars[n].desc, MavLinkVars[n].min, MavLinkVars[n].max, MavLinkVars[n].onload);
			strcat(content, tmp_str);
		}
	}
	strcat(content, "</TABLE><BR><BR>\n");
	strcat(content, "</TD></TR></TABLE>\n");
	webserv_html_stop(content);
}


void mavlink_web_get (char *url, char *content, char *type) {
	char tmp_str[512];
	if (strncmp(url, "/mavlink_value_set?", 19) == 0) {
		char name[20];
		float value = 0.0;
		int ntype = 0;
		sscanf(url + 19, "%[0-9a-zA-Z_]=%f&%i", name, &value, &ntype);
		mavlink_set_value(name, value, ntype, -1);
		mavlink_send_value(name, value, ntype);
		sprintf(content, "mavlink set value: %s to %f (type:%i)\n", name, value, ntype);
		strcpy(type, "text/plain");
	} else if (strncmp(url, "/mavlink.html?", 14) == 0) {
		char sub[128];
		sscanf(url + 14, "%[0-9a-zA-Z_]", sub);
		mavlink_html_page(content, sub);
		strcpy(type, "text/html");
	} else if (strncmp(url, "/mavlink.html", 13) == 0) {
		mavlink_html_page(content, "");
		strcpy(type, "text/html");
	} else if (strncmp(url, "/mavlink_value_get", 18) == 0) {
		uint16_t n = 0;
		strcpy(content, "# MAV ID  COMPONENT ID  PARAM NAME  VALUE (FLOAT) TYPE (INT)\n");
		for (n = 0; n < MAVLINK_PARAMETER_MAX; n++) {
			if (MavLinkVars[n].name[0] != 0) {
				sprintf(tmp_str, "%i %i %s %f %i\n", ModelData[ModelActive].sysid, ModelData[ModelActive].compid, MavLinkVars[n].name, MavLinkVars[n].value, MavLinkVars[n].type);
				strcat(content, tmp_str);
			}
		}
		strcpy(type, "text/plain");
	}
}

#ifndef WINDOWS

static int tcp_sock = -1;

int mavlink_tcp_send (uint8_t *buf, uint16_t len) {
	send(tcp_sock, buf, len, 0);
	return 0;
}

int mavlink_tcp (void *data) {
    struct sockaddr_in server;
	mavlink_message_t msg;
	mavlink_status_t status;
	SDL_Log("mavlink: init tcp thread\n");
    tcp_sock = socket(AF_INET , SOCK_STREAM , 0);
    if (tcp_sock == -1) {
        perror("mavlink_tcp: Could not create socket");
        return -1;
    }
    server.sin_addr.s_addr = inet_addr(setup.mavlink_tcp_server);
    server.sin_family = AF_INET;
    server.sin_port = htons(setup.mavlink_tcp_port);
    if (connect(tcp_sock , (struct sockaddr *)&server , sizeof(server)) < 0) {
        SDL_Log("mavlink_tcp: connection failed (%s:%i)\n", setup.mavlink_tcp_server, setup.mavlink_tcp_port);
        return -1;
    }
	while (gui_running == 1) {
		char buf[TCP_BUFLEN + 1];
		fd_set fds;
		struct timeval ts;
		ts.tv_sec = 1;
		ts.tv_usec = 0;
		FD_ZERO(&fds);
		if (tcp_sock != 0) {
			FD_SET(tcp_sock, &fds);
		}
		FD_SET(0, &fds);
		int nready = select(tcp_sock + 1, &fds, (fd_set *)0, (fd_set *)0, &ts);
		if (nready < 0) {
			SDL_Log("mavlink_tcp: error");
			break;
		} else if (nready == 0) {
			ts.tv_sec = 1;
			ts.tv_usec = 0;
		} else if (tcp_sock != 0 && FD_ISSET(tcp_sock, &fds)) {
			int rv;
			if ((rv = recv(tcp_sock , buf , TCP_BUFLEN , 0)) < 0) {
				return 1;
			} else if (rv == 0) {
				SDL_Log("mavlink_tcp: connection closed by the remote end\n");
				return 0;
			}

			int n = 0;
			for (n = 0; n < rv; n++) {
				if(mavlink_parse_char(1, buf[n], &msg, &status)) {
					mavlink_handleMessage(&msg);
					mavlink_tcp_active = 1;
				}
			}
			fflush(0);
		}
		SDL_Delay(1);
	}
	SDL_Log("mavlink: exit tcp thread\n");
	return 0;
}

int mavlink_udp (void *data) {
	mavlink_message_t msg;
	mavlink_status_t status;
	char buf[UDP_BUFLEN];
	SDL_Log("mavlink: init udp thread\n");
	if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		SDL_Log("mavlink: socket error\n");
		return 0;
	}
	int flags = fcntl(s, F_GETFL);
	flags |= O_NONBLOCK;
	fcntl(s, F_SETFL, flags);
	memset((char *) &si_me, 0, sizeof(si_me));
	si_me.sin_family = AF_INET;
	si_me.sin_port = htons(setup.mavlink_udp_port);
	si_me.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(s , (struct sockaddr*)&si_me, sizeof(si_me)) == -1) {
		SDL_Log("mavlink: bind error\n");
		return 0;
	}
	while (udp_running == 1) {
		fflush(stdout);
		while ((recv_len = recvfrom(s, buf, UDP_BUFLEN, 0, (struct sockaddr *)&si_other, (socklen_t *)&slen)) != -1) {
			int n = 0;
			for (n = 0; n < recv_len; n++) {
				if(mavlink_parse_char(1, buf[n], &msg, &status)) {
					mavlink_handleMessage(&msg);
					mavlink_udp_active = 1;
				}
			}
		}
		SDL_Delay(1);
	}
	close(s);
	SDL_Log("mavlink: exit udp thread\n");
	return 0;
}
#else
int mavlink_udp (void *data) {
	return 0;
}

int mavlink_tcp (void *data) {
	return 0;
}
#endif
