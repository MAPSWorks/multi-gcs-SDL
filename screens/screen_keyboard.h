
extern void screen_keyboard (ESContext *esContext);

void keyboard_set_mode (uint8_t mode);
uint8_t keyboard_get_mode (void);
void keyboard_set_text (char *text);
char *keyboard_get_text (void);
void keyboard_set_callback (uint8_t (*callback) (char *, float, float, int8_t, float));
