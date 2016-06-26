#include "cpu/reg.h"
#include "monitor/monitor.h"
#include "monitor/expr.h"
#include "monitor/watchpoint.h"
#include "nemu.h"

#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

void cpu_exec(uint32_t);

uint32_t hwaddr_read(hwaddr_t,size_t);

/* We use the ``readline'' library to provide more flexibility to read from stdin. */

char* rl_gets() {
	static char *line_read = NULL;

	if (line_read) {
		free(line_read);
		line_read = NULL;
	}

	line_read = readline("(nemu) ");

	if (line_read && *line_read) {
		add_history(line_read);
	}

	return line_read;
}

int chartoint(char *str){
	int len = strlen(str);
	int re = 0, m = 1, i;
	for( i = len - 1; i >= 0; i--){
		re = re + m * (str[i] - '0');
		m = m * 10;
	}
	return re;
}

static int cmd_c(char *args) {
	cpu_exec(-1);
	return 0;
}

static int cmd_q(char *args) {
	return -1;
}

static int cmd_help(char *args);

static int cmd_si(char *args) {
	int i = 0, num = 0;
	if (args == NULL)
		cpu_exec(1);
	else {
		int len = strlen(args);
		for (i = 0; i < len; ++ i)
			num = (num * 10 + args[i] - '0');
		if (num <= 0)
			cpu_exec(1);
	}
	cpu_exec(num);
	return 0;
}


static int cmd_info(char *args) {
	int i;
	if(strcmp(args, "r") == 0) {
		for(i=0;i<8;i++){
			printf("%s-%08x\n", regsl[i], reg_l(i));
		}

		for(i=0;i<8;i++){
			printf("%s-%04x\n", regsw[i], reg_w(i));
		}

		for(i=0;i<8;i++){
			printf("%s-%02x\n", regsb[i], reg_b(i));
		}
		printf("eip-%x\n", cpu.eip);		
	}
	else if( strcmp(args, "w") == 0 ){
		WP* temp = head;
		while(temp != NULL){
			printf("%d, %s, %d\n", temp->NO, temp->addr, temp->old_value);
			temp = temp->next;
		}
	}
	else
		printf("Something's wrong:-(\n");

	return 0;
}

static int cmd_x(char *args)
{
	char *arg = strtok(NULL," ");
	int n = chartoint(args);
	arg = arg + strlen(arg) + 1;
	int len = strlen(arg);
	int i, m, re=0;
	for(i = len - 1, m = 1; len >= 2; len--){
		re = re + (arg[i] - '0') * m;
		m = m << 4;
	}
	for(i = 0; i < n;i++)
    		printf("0x%x\n", hwaddr_read(re + i, 4));
	return 0;
}


static int cmd_p(char *args) {
	bool *flag = malloc(sizeof(bool));
	uint32_t expression = expr(args, flag); 
	//实现表达式求值即调用expr.c
	if (*flag == true) printf("%s = 0x%x\n", args, expression);
	else printf("Invalid!");
	return 0;
}

static int cmd_w(char *args) {
	int i = 0;
	bool *flag = malloc(sizeof(bool));
	WP *wp = new_wp();
	if (args[0] != '*' && args[0] != '$') {
		printf("Wrong input!\n");
		return 0;
	}
	printf("Watchpoint No.%d: expr:%s\n", wp->NO, args);
	while (args[i] != 0) {
		wp->addr[i] = args[i];
		++i;
	}
	wp->old_value = expr(wp->addr, flag);

	return 0;
}

static int cmd_d(char *args) {

	bool *flag = malloc(sizeof(bool));
	int no = expr(args, flag);
	WP* temp = head;
	while(temp != NULL){
		if(temp->NO == no){
			free_wp(temp);
			printf("Delete watchpoint NO:%d\n", temp->NO);
			break;
		}
		temp = temp->next;
	}
	return 0;
}

//static int cmd_bt(char *args);

static struct {
	char *name;
	char *description;
	int (*handler) (char *);
} cmd_table [] = {
	{ "help", "Display informations about all supported commands", cmd_help },
	{ "c", "Continue the execution of the program", cmd_c },
	{ "q", "Exit NEMU", cmd_q },
	{ "si","Single-step", cmd_si}, //单步执行
	{ "info","Print status", cmd_info}, //打印程序状态
	{ "p","Calculate the experssion", cmd_p}, //表达式求值 PA1-2
	{ "x","Scan memory", cmd_x}, //扫描内存
	{ "w","Add a watchpoint", cmd_w}, //设置监视 PA1-3
	{ "d","Delete a watchpoint", cmd_d}, //删除监视 PA1-3
//	{ "bt","Print the stack", cmd_bt}, //打印栈帧链 PA2

};

#define NR_CMD (sizeof(cmd_table) / sizeof(cmd_table[0]))

static int cmd_help(char *args) {
	/* extract the first argument */
	char *arg = strtok(NULL, " ");
	int i;

	if(arg == NULL) {
		/* no argument given */
		for(i = 0; i < NR_CMD; i ++) {
			printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
		}
	}
	else {
		for(i = 0; i < NR_CMD; i ++) {
			if(strcmp(arg, cmd_table[i].name) == 0) {
				printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
				return 0;
			}
		}
		printf("Unknown command '%s'\n", arg);
	}
	return 0;
}

void ui_mainloop() {
	while(1) {
		char *str = rl_gets();
		char *str_end = str + strlen(str);

		/* extract the first token as the command */
		char *cmd = strtok(str, " ");
		if(cmd == NULL) { continue; }

		/* treat the remaining string as the arguments,
		 * which may need further parsing
		 */
		char *args = cmd + strlen(cmd) + 1;
		if(args >= str_end) {
			args = NULL;
		}

#ifdef HAS_DEVICE
		extern void sdl_clear_event_queue(void);
		sdl_clear_event_queue();
#endif

		int i;
		for(i = 0; i < NR_CMD; i ++) {
			if(strcmp(cmd, cmd_table[i].name) == 0) {
				if(cmd_table[i].handler(args) < 0) { return; }
				break;
			}
		}

		if(i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
	}
}
