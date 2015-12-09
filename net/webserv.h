

void webserv_init(void);
void webserv_exit(void);
uint8_t webserv_is_running(void);

void webserv_html_head(char *content, char *title);
void webserv_html_start(char *content, uint8_t init);
void webserv_html_stop(char *content);

