
#include <stdint.h>
#include <stdio.h>
#include <libgen.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <time.h>
#include <sys/times.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <math.h>
#include <userdata.h>
#include <model.h>
#include <serial.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <model.h>
#include <main.h>
#include <tracker.h>
#include <screen_tracker.h>

#define SLIDER_START	-0.2
#define SLIDER_LEN	1.4

uint8_t tracker_cmd (char *name, float x, float y, int8_t button, float data) {
	printf("#null# %s %f\n", name, data);
	if (strcmp(name + 1, "PAN_TRIM") == 0) {
		TrackerData[TRACKER_PAN_TRIM].value = tracker_pan_dir;
	} else if (strcmp(name + 1, "PITCH_TRIM") == 0) {
		TrackerData[TRACKER_PITCH_TRIM].value = tracker_pitch_dir;
	}
	return 0;
}

uint8_t tracker_home (char *name, float x, float y, int8_t button, float data) {
	tracker_set_home();
	return 0;
}


uint8_t tracker_move (char *name, float x, float y, int8_t button, float data) {
	int n = 0;
	float value = 0.0;
	float min = -100.0;
	float max = 100.0;

	min = TrackerData[(int)data].min;
	max = TrackerData[(int)data].max;
	value = TrackerData[(int)data].value;

	if (button == 4) {
	} else if (button == 5) {
	} else {
		float percent = (x - SLIDER_START) * 100.0 / SLIDER_LEN;
		if (percent > 100.0) {
			percent = 100.0;
		} else if (percent < 0.0) {
			percent = 0.0;
		}
		float diff = max - min;
		float new = percent * diff / 100.0 + min;
		printf("slider: %s %f %f %f %f\n", name + 1, x, percent, new, data);
		if (strstr(name, "baud") > 0) {
			float bauds[] = {1200.0, 2400.0, 9600.0, 38400.0, 57600.0, 115200.0, 200000.0};
			for (n = 0; n < 6; n++) {
				if (new <= bauds[n] + (bauds[n + 1] - bauds[n]) / 2) {
					new = bauds[n];
					break;
				}
			}
		}
		value = new;
	}
	if (value < min) {
		value = min;
	} else if (value > max) {
		value = max;
	}

	TrackerData[(int)data].value = value;

	return 0;
}


void screen_tracker (ESContext *esContext) {
	char tmp_str[1024];
	char tmp_str2[1024];
	uint8_t n = 0;
#ifndef SDLGL
	ESMatrix modelview;
	UserData *userData = esContext->userData;
#endif
	draw_title(esContext, "Antenna-Tracker");
#ifndef SDLGL
	esMatrixLoadIdentity(&modelview);
	esMatrixMultiply(&userData->mvpMatrix, &modelview, &userData->perspective);
	esMatrixMultiply(&userData->mvpMatrix2, &modelview, &userData->perspective);
#endif

	for (n = 0; n < TRACKER_MAX; n++) {
		draw_box_f3c2(esContext, SLIDER_START, -0.75 + n * 0.15, 0.002, SLIDER_START + SLIDER_LEN, -0.75 + n * 0.15 + 0.1, 0.002, 55, 55, 55, 220, 75, 45, 85, 100);


		draw_box_f3c2(esContext, SLIDER_START + SLIDER_LEN / 2.0, -0.75 + n * 0.15, 0.002, SLIDER_START + ((TrackerData[n].value - TrackerData[n].min) * SLIDER_LEN / (TrackerData[n].max - TrackerData[n].min)), -0.75 + n * 0.15 + 0.1, 0.002, 255, 255, 55, 220, 175, 145, 85, 100);

		if (strcmp(TrackerData[n].name, "PAN_TRIM") == 0) {
			draw_box_f3c2(esContext, SLIDER_START + SLIDER_LEN / 2.0, -0.75 + n * 0.15, 0.002, SLIDER_START + ((tracker_pan_dir - TrackerData[n].min) * SLIDER_LEN / (TrackerData[n].max - TrackerData[n].min)), -0.75 + n * 0.15 + 0.05, 0.002, 255, 255, 55, 220, 255, 145, 85, 100);
			draw_box_f3c2(esContext, SLIDER_START + SLIDER_LEN / 2.0, -0.75 + n * 0.15 + 0.05, 0.002, SLIDER_START + ((tracker_pan_dir_trimmed - TrackerData[n].min) * SLIDER_LEN / (TrackerData[n].max - TrackerData[n].min)), -0.75 + n * 0.15 + 0.1, 0.002, 255, 255, 55, 220, 175, 145, 255, 100);
		} else if (strcmp(TrackerData[n].name, "PITCH_TRIM") == 0) {
			draw_box_f3c2(esContext, SLIDER_START + SLIDER_LEN / 2.0, -0.75 + n * 0.15, 0.002, SLIDER_START + ((tracker_pitch_dir - TrackerData[n].min) * SLIDER_LEN / (TrackerData[n].max - TrackerData[n].min)), -0.75 + n * 0.15 + 0.05, 0.002, 255, 255, 55, 220, 255, 145, 85, 100);
			draw_box_f3c2(esContext, SLIDER_START + SLIDER_LEN / 2.0, -0.75 + n * 0.15 + 0.05, 0.002, SLIDER_START + ((tracker_pitch_dir_trimmed - TrackerData[n].min) * SLIDER_LEN / (TrackerData[n].max - TrackerData[n].min)), -0.75 + n * 0.15 + 0.1, 0.002, 255, 255, 55, 220, 175, 145, 255, 100);
		} else if (strcmp(TrackerData[n].name, "PAN_ANGLE_MIN") == 0 || strcmp(TrackerData[n].name, "PAN_ANGLE_MAX") == 0) {
			draw_box_f3c2(esContext, SLIDER_START + SLIDER_LEN / 2.0, -0.75 + n * 0.15, 0.002, SLIDER_START + ((tracker_pan_dir_trimmed - TrackerData[n].min) * SLIDER_LEN / (TrackerData[n].max - TrackerData[n].min)), -0.75 + n * 0.15 + 0.05, 0.002, 255, 255, 55, 220, 255, 145, 85, 100);
		} else if (strcmp(TrackerData[n].name, "PITCH_ANGLE_MIN") == 0 || strcmp(TrackerData[n].name, "PITCH_ANGLE_MAX") == 0) {
			draw_box_f3c2(esContext, SLIDER_START + SLIDER_LEN / 2.0, -0.75 + n * 0.15, 0.002, SLIDER_START + ((tracker_pitch_dir_trimmed - TrackerData[n].min) * SLIDER_LEN / (TrackerData[n].max - TrackerData[n].min)), -0.75 + n * 0.15 + 0.05, 0.002, 255, 255, 55, 220, 255, 145, 85, 100);
		} else if (strcmp(TrackerData[n].name, "PAN_PULSE_MIN") == 0 || strcmp(TrackerData[n].name, "PAN_PULSE_MAX") == 0) {
			float val = (tracker_pan_dir_trimmed - TrackerData[TRACKER_PAN_ANGLE_MIN].value) * (TrackerData[TRACKER_PAN_PULSE_MAX].value - TrackerData[TRACKER_PAN_PULSE_MIN].value) / (TrackerData[TRACKER_PAN_ANGLE_MAX].value - TrackerData[TRACKER_PAN_ANGLE_MIN].value) + TrackerData[TRACKER_PAN_PULSE_MIN].value;
			draw_box_f3c2(esContext, SLIDER_START + SLIDER_LEN / 2.0, -0.75 + n * 0.15, 0.002, SLIDER_START + ((val - TrackerData[n].min) * SLIDER_LEN / (TrackerData[n].max - TrackerData[n].min)), -0.75 + n * 0.15 + 0.05, 0.002, 255, 255, 55, 220, 255, 145, 85, 100);
		} else if (strcmp(TrackerData[n].name, "PITCH_PULSE_MIN") == 0 || strcmp(TrackerData[n].name, "PITCH_PULSE_MAX") == 0) {
			float val = (tracker_pitch_dir_trimmed - TrackerData[TRACKER_PITCH_ANGLE_MIN].value) * (TrackerData[TRACKER_PITCH_PULSE_MAX].value - TrackerData[TRACKER_PITCH_PULSE_MIN].value) / (TrackerData[TRACKER_PITCH_ANGLE_MAX].value - TrackerData[TRACKER_PITCH_ANGLE_MIN].value) + TrackerData[TRACKER_PITCH_PULSE_MIN].value;
			draw_box_f3c2(esContext, SLIDER_START + SLIDER_LEN / 2.0, -0.75 + n * 0.15, 0.002, SLIDER_START + ((val - TrackerData[n].min) * SLIDER_LEN / (TrackerData[n].max - TrackerData[n].min)), -0.75 + n * 0.15 + 0.05, 0.002, 255, 255, 55, 220, 255, 145, 85, 100);
		}

		sprintf(tmp_str, "S%s", TrackerData[n].name);
		set_button(tmp_str, view_mode, SLIDER_START, -0.75 + n * 0.15, SLIDER_START + SLIDER_LEN, -0.75 + n * 0.15 + 0.1, tracker_move, (float)n, 1);

		sprintf(tmp_str, "T%s", TrackerData[n].name);
		sprintf(tmp_str2, "%s: %0.1f", TrackerData[n].name, TrackerData[n].value);
		draw_button(esContext, tmp_str, view_mode, tmp_str2, FONT_WHITE, -1.2, -0.75 + n * 0.15 + 0.02, 0.003, 0.06, ALIGN_LEFT, ALIGN_TOP, tracker_cmd, 0.0);
	}


	draw_button(esContext, "set_home", view_mode, "SET_HOME", FONT_WHITE, 0.0, 0.9, 0.002, 0.06, ALIGN_CENTER, ALIGN_TOP, tracker_home, 0.0);
}
