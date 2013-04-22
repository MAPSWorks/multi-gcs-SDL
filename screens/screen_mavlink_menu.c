
#include <userdata.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <main.h>
#include <model.h>
#include <screen_mavlink_menu.h>
#include <screen_keyboard.h>
#include <screen_filesystem.h>
#include <screen_rctransmitter.h>
#include <i2c.h>
#include <my_mavlink.h>

float sel = 0.0;
float set_sel = 0.0;
float sel2 = 0.0;
float set_sel2 = 0.0;
uint8_t sel1_mode = 0;
uint8_t sel2_mode = 0;
char select_section[100];

uint8_t mavlink_param_file_save (char *name, float x, float y, int8_t button, float data) {
	printf("# saving file: %s #\n", name);
	return 0;
}

uint8_t mavlink_param_save (char *name, float x, float y, int8_t button, float data) {
	keyboard_set_callback(mavlink_param_file_save);
	keyboard_set_text("test-file");
	keyboard_set_mode(VIEW_MODE_FCMENU);
	return 0;
}

uint8_t mavlink_param_file_load (char *name, float x, float y, int8_t button, float data) {
	mavlink_param_read_file(name);
	return 0;
}

uint8_t mavlink_param_load (char *name, float x, float y, int8_t button, float data) {
	char directory[200];
	sprintf(directory, "%s/PARAMS", BASE_DIR);
	filesystem_set_callback(mavlink_param_file_load);
	filesystem_set_dir(directory);
	filesystem_reset_filter();
	filesystem_add_filter(".txt\0");
	filesystem_add_filter(".param\0");
	filesystem_set_mode(VIEW_MODE_FCMENU);
	return 0;
}

uint8_t mavlink_flash (char *name, float x, float y, int8_t button, float data) {
	save_to_flash();
	return 0;
}

uint8_t mavlink_select_main (char *name, float x, float y, int8_t button, float data) {
	sel2 = (float)data;
	set_sel2 = (float)data;

	strcpy(select_section, name + 8);

	sel1_mode = 1;
	reset_buttons();
	return 0;
}

uint8_t mavlink_select_sel (char *name, float x, float y, int8_t button, float data) {
	uint16_t n = 0;
	printf("%s -- %0.2f\n", name + 1, data);
	if (data == 0.0) {
		sel1_mode = 0;
		reset_buttons();
		return 0;
	}
	for (n = 0; n < 500 - 1; n++) {
		if (strcmp(MavLinkVars[n].name, name + 1) == 0) {
			MavLinkVars[n].value += data;
			mavlink_send_value(MavLinkVars[n].name, MavLinkVars[n].value);
			break;
		}
	}
	return 0;
}

uint8_t mavlink_select_sel_scroll (char *name, float x, float y, int8_t button, float data) {
	if ((int)data > 0) {
		set_sel++;
	} else if (set_sel > 0) {
		set_sel--;
	}
	return 0;
}

void mavlink_param_read_file (char *param_file) {
        FILE *fr;
        char line[1024];
        int tmp_int1;
        int tmp_int2;
        char var1[101];
        char val[101];
        fr = fopen (param_file, "r");
        while(fgets(line, 100, fr) != NULL) {
                var1[0] = 0;
                val[0] = 0;
		if (line[0] != '#' && line[0] != '\n') {
	                sscanf (line, "%i %i %s %s", &tmp_int1, &tmp_int2, (char *)&var1, (char *)&val);
			float new_val = atof(val);
//	                printf ("#%s# = %f\n", var1, new_val);
			uint16_t n = 0;
			uint8_t flag = 0;
			for (n = 0; n < 500; n++) {
				if (strcmp(MavLinkVars[n].name, var1) == 0) {
					float old_val = MavLinkVars[n].value;
					if (old_val != new_val) {
						printf ("CHANGED: %s = %f (OLD: %f)\n", var1, new_val, MavLinkVars[n].value);
						MavLinkVars[n].value = atof(val);
					}
					flag = 1;
					break;
				}
			}
			if (flag == 0) {
				for (n = 0; n < 500; n++) {
					if (MavLinkVars[n].name[0] == 0) {
						strcpy(MavLinkVars[n].name, var1);
						MavLinkVars[n].value = atof(val);
						MavLinkVars[n].id = -1;
				                printf ("NEW: %s = %f\n", var1, atof(val));
						break;
					}
				}
			}

		}
        }
        fclose(fr);
}

void mavlink_param_upload_all (char *param_file) {
        FILE *fr;
        char line[1024];
        int tmp_int1;
        int tmp_int2;
        char var[101];
        char val[101];
        fr = fopen (param_file, "r");
        while(fgets(line, 100, fr) != NULL) {
                var[0] = 0;
                val[0] = 0;
		if (line[0] != '#' && line[0] != '\n') {
	                sscanf (line, "%i %i %s %s", &tmp_int1, &tmp_int2, (char *)&var, (char *)&val);
	                printf ("%s = %f\n", var, atof(val));
			mavlink_send_value(var, atof(val));
			SDL_Delay(20);
		}
        }
        fclose(fr);
}


void screen_mavlink_menu (ESContext *esContext) {
#ifndef SDLGL
	ESMatrix modelview;
	UserData *userData = esContext->userData;
#endif
	int16_t row = 0;
	int16_t col = 0;
	int16_t row2 = 0;
	int16_t n = 0;
	char section[100];
	char tmp_str[100];
	char tmp_str2[100];
	int8_t flag = 0;
	int8_t flag2 = 0;
	draw_title(esContext, "MavLink");
	if (i2c_button1 == 1) {
		sel1_mode = 1 - sel1_mode;
		printf("#i2c_button1#\n");
		usleep(10000);
	}
	if (i2c_button2 == 1) {
		sel2_mode = 1 - sel2_mode;
		printf("#i2c_button2#\n");
		usleep(10000);
	}
	if (sel1_mode == 0) {
		if (i2c_diff1 > 0 && set_sel2 > 0) {
			set_sel2--;
		} else if (i2c_diff1 < 0) {
			set_sel2++;
		}
	} else {
		if (i2c_diff1 > 0 && set_sel > 0) {
			set_sel--;
		} else if (i2c_diff1 < 0) {
			set_sel++;
		}
	}

	if (sel1_mode == 1) {
		if (sel2_mode == 0) {
			if (i2c_diff2 > 0) {
				MavLinkVars[selMavLinkVars[(int)sel].id].value += 0.1;
			} else if (i2c_diff2 < 0) {
				MavLinkVars[selMavLinkVars[(int)sel].id].value -= 0.1;
			}
		} else {
			if (i2c_diff2 > 0) {
				MavLinkVars[selMavLinkVars[(int)sel].id].value += 1.0;
			} else if (i2c_diff2 < 0) {
				MavLinkVars[selMavLinkVars[(int)sel].id].value -= 1.0;
			}
		}
	}
/*
	if (sel2 < set_sel2 - 0.05) {
		sel2 += 0.2;
		set_sel = 0.0;
	} else if (sel2 > set_sel2 + 0.05) {
		sel2 -= 0.2;
		set_sel = 0.0;
	} else {
		sel2 = set_sel2;
	}
	if (sel < set_sel - 0.05) {
		sel += 0.2;
	} else if (sel > set_sel + 0.05) {
		sel -= 0.2;
	} else {
		sel = set_sel;
	}
*/
	strcpy(section, mainMavLinkVars[(int)sel2].name);
printf("### %s %f\n", section, sel2);
	row2 = 0;
	for (row = 0; row < 500 - 1; row++) {
		if (strlen(MavLinkVars[row].name) > 3) {
			strcpy(tmp_str, MavLinkVars[row].name);
			for (n = 0; n < strlen(tmp_str) ; n++) {
				if (tmp_str[n] == '_') {
					tmp_str[n] = 0;
					break;
				}
			}
			flag2 = 0;
			for (row2 = 0; row2 < 500 - 1; row2++) {
				if (strcmp(mainMavLinkVars[row2].name, tmp_str) == 0) {
					flag2 = 1;
					break;
				}
			}
			if (flag2 == 0) {
				for (row2 = 0; row2 < 500 - 1; row2++) {
					if (mainMavLinkVars[row2].name[0] == 0) {
						strcpy(mainMavLinkVars[row2].name, tmp_str);
						break;
					}
				}
			}
			flag = 1;
		}
	}
	if (row2 > 0 && set_sel2 >= row2) {
		set_sel2 = row2 - 1;
	}
	row2 = 0;
	for (row = 0; row < 500 - 1; row++) {
		if (strncmp(MavLinkVars[row].name, section, strlen(section)) == 0) {
//			printf("%s - %s\n", section, MavLinkVars[row].name);
			strcpy(selMavLinkVars[row2].name, MavLinkVars[row].name);
			selMavLinkVars[row2].value = MavLinkVars[row].value;
			selMavLinkVars[row2].id = row;
			selMavLinkVars[row2 + 1].name[0] = 0;
			selMavLinkVars[row2 + 1].value = 0.0;
			selMavLinkVars[row2 + 1].id = 0;
			row2++;
		}
	}
	if (row2 > 0 && set_sel >= row2) {
		set_sel = row2 - 1;
	}

	if (sel1_mode != 1) {
		col = 0;
		row = 0;
		for (n = 0; n < 500; n++) {
			if (mainMavLinkVars[n].name[0] == 0) {
				break;
			}
			sprintf(tmp_str, "mv_main_%s", mainMavLinkVars[n].name);
			draw_button(esContext, tmp_str, VIEW_MODE_FCMENU, mainMavLinkVars[n].name, FONT_WHITE, -1.3 + col * 0.5, -0.8 + row * 0.12, 0.002, 0.08, 0, 0, mavlink_select_main, n);
			col++;
			if (col > 4) {
				col = 0;
				row++;
			}
		}
	}
	if (sel1_mode == 1) {
		col = 0;
		row = 0;
		if ((int)set_sel > 0) {
			sprintf(tmp_str, "-scroll_%s", selMavLinkVars[n].name);
			draw_button(esContext, tmp_str, VIEW_MODE_FCMENU, "[^]", FONT_WHITE, 0.0, -0.7 - 0.14, 0.002, 0.08, 1, 0, mavlink_select_sel_scroll, -1.0);
		}

		for (n = 0; n < 10 && n + (int)set_sel < 500; n++) {
			if (selMavLinkVars[n + (int)set_sel].name[0] == 0) {
				break;
			}

			if (strcmp(selMavLinkVars[n + (int)set_sel].name, diff_name[0]) == 0) {
				sprintf(tmp_str, "RESET 0%i", n);
				draw_button(esContext, tmp_str, VIEW_MODE_FCMENU, "1", FONT_GREEN, -1.4, -0.7 + row * 0.14, 0.002, 0.08, 0, 0, rctransmitter_mavlink_diff, 0);
			} else {
				sprintf(tmp_str, "%s 0%i", selMavLinkVars[n + (int)set_sel].name, n);
				draw_button(esContext, tmp_str, VIEW_MODE_FCMENU, "1", FONT_WHITE, -1.4, -0.7 + row * 0.14, 0.002, 0.08, 0, 0, rctransmitter_mavlink_diff, 0);
			}
			if (strcmp(selMavLinkVars[n + (int)set_sel].name, diff_name[1]) == 0) {
				sprintf(tmp_str, "RESET 1%i", n);
				draw_button(esContext, tmp_str, VIEW_MODE_FCMENU, "2", FONT_GREEN, -1.3, -0.7 + row * 0.14, 0.002, 0.08, 0, 0, rctransmitter_mavlink_diff, 1);
			} else {
				sprintf(tmp_str, "%s 1%i", selMavLinkVars[n + (int)set_sel].name, n);
				draw_button(esContext, tmp_str, VIEW_MODE_FCMENU, "2", FONT_WHITE, -1.3, -0.7 + row * 0.14, 0.002, 0.08, 0, 0, rctransmitter_mavlink_diff, 1);
			}

			sprintf(tmp_str, "mv_sel_%s_%i_t", selMavLinkVars[n + (int)set_sel].name, n);
			draw_button(esContext, tmp_str, VIEW_MODE_FCMENU, selMavLinkVars[n + (int)set_sel].name, FONT_WHITE, -1.2, -0.7 + row * 0.14, 0.002, 0.08, 0, 0, mavlink_select_sel, n);

			sprintf(tmp_str, "mv_sel_%s_%i_v", selMavLinkVars[n + (int)set_sel].name, n);
			sprintf(tmp_str2, "%0.4f", selMavLinkVars[n + (int)set_sel].value);
			draw_button(esContext, tmp_str, VIEW_MODE_FCMENU, tmp_str2, FONT_WHITE, 0.05, -0.7 + row * 0.14, 0.002, 0.08, 2, 0, mavlink_select_sel, n);

			sprintf(tmp_str, "a%s", selMavLinkVars[n + (int)set_sel].name);
			draw_button(esContext, tmp_str, VIEW_MODE_FCMENU, "[-1.0]", FONT_WHITE, 0.1, -0.7 + row * 0.14, 0.002, 0.08, 0, 0, mavlink_select_sel, -1.0);

			sprintf(tmp_str, "b%s", selMavLinkVars[n + (int)set_sel].name);
			draw_button(esContext, tmp_str, VIEW_MODE_FCMENU, "[-0.1]", FONT_WHITE, 0.4, -0.7 + row * 0.14, 0.002, 0.08, 0, 0, mavlink_select_sel, -0.1);

			sprintf(tmp_str, "c%s", selMavLinkVars[n + (int)set_sel].name);
			draw_button(esContext, tmp_str, VIEW_MODE_FCMENU, "[+0.1]", FONT_WHITE, 0.7, -0.7 + row * 0.14, 0.002, 0.08, 0, 0, mavlink_select_sel, 0.1);

			sprintf(tmp_str, "d%s", selMavLinkVars[n + (int)set_sel].name);
			draw_button(esContext, tmp_str, VIEW_MODE_FCMENU, "[+1.0]", FONT_WHITE, 1.0, -0.7 + row * 0.14, 0.002, 0.08, 0, 0, mavlink_select_sel, 1.0);

			row++;
		}
		if (n >= 10) {
			sprintf(tmp_str, "+scroll_%s", selMavLinkVars[n + (int)set_sel].name);
			draw_button(esContext, tmp_str, VIEW_MODE_FCMENU, "[v]", FONT_WHITE, 0.0, 0.7, 0.002, 0.08, 1, 0, mavlink_select_sel_scroll, 1.0);
		}
		draw_button(esContext, "back", VIEW_MODE_FCMENU, "[BACK]", FONT_WHITE, -1.3, 0.8, 0.002, 0.08, 0, 0, mavlink_select_sel, 0.0);
	}


	draw_button(esContext, "flash", VIEW_MODE_FCMENU, "[WRITE TO FLASH]", FONT_WHITE, 0.0, 0.9, 0.002, 0.06, 1, 0, mavlink_flash, 0.0);
	draw_button(esContext, "load", VIEW_MODE_FCMENU, "[LOAD PARAM]", FONT_WHITE, -0.7, 0.9, 0.002, 0.06, 1, 0, mavlink_param_load, 1.0);
	draw_button(esContext, "save", VIEW_MODE_FCMENU, "[SAVE PARAM]", FONT_WHITE, 0.7, 0.9, 0.002, 0.06, 1, 0, mavlink_param_save, 1.0);

	if (flag == 0) {
		draw_text_f(esContext, -0.4, 0.0, 0.05, 0.05, FONT_BLACK_BG, "No Mavlink-Parameters found");
	}

	screen_keyboard(esContext);
	screen_filesystem(esContext);


}

