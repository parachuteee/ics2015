#include "nemu.h"

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <sys/types.h>
#include <regex.h>

bool flag = 1;
int Num_type = 10;

enum {
	NOTYPE = 256, DIGIT = 0x30, 
	HEX, EQ, NEQ,
	AND, OR, NOT,
	REG_NAME, REG, OBJ
};

extern uint32_t get_addr_obj(char*);

static struct rule {
	char *regex;
	int token_type;
} rules[] = {

	/* Add more rules.
	 * Pay attention to the precedence level of different rules.
	 */
	{" +",	NOTYPE},					// spaces
	{"0x", HEX},						// hex
	{"\\$", REG_NAME},					// reg name
	{"[e][abcd][x]|[e][sbi][p]|[e][sd][i]|[abcd][x]|[sb][p]|[sd][i]|[abcd][lh]", REG},								//registers
	{"[A-Za-z_][0-9A-Za-z_]+", OBJ},		// OBJ
	{"[0-9A-Fa-f]+", DIGIT},			// digits
	{"\\(", '('},						// left
	{"\\)", ')'},						// right
	{"\\*", '*'},						// multiple
	{"\\/", '/'},						// divide
	{"\\+", '+'},						// plus
	{"\\-", '-'},						// minus
	{"==", EQ},							// equal
	{"!=", NEQ},						// not equal
	{"[&]{2}", AND},					// and
	{"[|]{2}", OR},						// or
	{"\\!", NOT},						// not

};

static regex_t re[NR_REGEX];

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */

void init_regex() {
	int i;
	char error_msg[128];
	int ret;

	for(i = 0; i < NR_REGEX; i ++) {
		ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
		if(ret != 0) {
			regerror(ret, &re[i], error_msg, 128);
			Assert(ret == 0, "regex compilation failed: %s\n%s", error_msg, rules[i].regex);
		}
	}
}

typedef struct token {
	int type;
	char str[32];
} Token;

Token tokens[32];
int nr_token;

int check_parentheses(int, int);

int Judge_op(int, int);

uint32_t eval(int, int);

static bool make_token(char *e) {
	int position = 0;
	int i;
	regmatch_t pmatch;
	
	tokens[0].type = '(';
	nr_token = 1;

	while(e[position] != '\0') {
		for(i = 0; i < NR_REGEX; ++i) {
			if(regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
				int substr_len = pmatch.rm_eo;
				position += substr_len;

				/* Now a new token is recognized with rules[i]. Add codes
				 * to record the token in the array ``tokens''. For certain 
				 * types of tokens, some extra actions should be performed.
				 */

				switch(rules[i].token_type) {
					int j;
					case NOTYPE: break;
					case OBJ:
							position -= substr_len;
							j = 0;
							while (e[position] != '\0' && ((e[position] >= 'a' && e[position] <= 'z') || (e[position] >= 'A' && e[position] <= 'Z') || (e[position] >= '0' && e[position] <= '9') || e[position] == '_')) {
								tokens[nr_token].str[j ++] = e[(position ++)];
							}
							tokens[nr_token].str[j] = '\0';

							if (get_addr_obj(tokens[nr_token].str)) {
								tokens[nr_token].type = OBJ;
								++ nr_token;
							}
							else {
								printf("object not exist!\n");
							} 
							break;					
					case DIGIT:
							position -= substr_len;
							tokens[nr_token].type = DIGIT;
							j = 0;
							while (e[position] != '\0' && j < 32 && ((e[position] <= '9' && e[position] >= '0') || (e[position] <= 'Z' && e[position] >= 'A') || (e[position] >= 'a' && e[position] <= 'z'))) 
								tokens[nr_token].str[j ++] = e[(position ++)];
							tokens[nr_token].str[j] = '\0';
							if (j >= 32 && !((e[position] <= '9' && e[position] >= '0') || (e[position] <= 'Z' && e[position] >= 'A') || (e[position] >= 'a' && e[position] <= 'z'))) {
								j ++;
								assert(0);
							}
							++ nr_token; 
							break; 
					case HEX: tokens[nr_token ++].type = HEX; break;
					case EQ: tokens[nr_token ++].type = EQ; break;
					case NEQ: tokens[nr_token ++].type = NEQ; break;
					case '(': tokens[nr_token ++].type = '('; break;
					case ')': tokens[nr_token ++].type = ')'; break;
					case '*': tokens[nr_token ++].type = '*'; break;
					case '/': tokens[nr_token ++].type = '/'; break;
					case '+': tokens[nr_token ++].type = '+'; break;
					case '-': tokens[nr_token ++].type = '-'; break;
					case AND: tokens[nr_token ++].type = AND; break;
					case OR: tokens[nr_token ++].type = OR; break;
					case NOT: tokens[nr_token ++].type = NOT; break;
					case REG_NAME: tokens[nr_token ++].type = REG_NAME; break; 
					case REG:
							position -= substr_len;
							tokens[nr_token].type = REG;
							j = 0;
							while (j < 32 && ((e[position]>='A' && e[position]<='Z') || (e[position]>='a' && e[position]<='z'))) 
								tokens[nr_token].str[j ++] = e[(position ++)];
							tokens[nr_token].str[j] = '\0';
							++nr_token; 
							break; 

					default: panic("please implement me");
				}

				break;
			}
		}

		if(i == NR_REGEX) {
			printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
			return false;
		}
	}
	tokens[nr_token ++].type = ')';
	return true;
}

uint32_t expr(char *e, bool *flag) {
	if(!make_token(e)) {
		*flag = false;
		return 0;
	}
	/* Insert codes to evaluate the expression. */
	*flag = true;
	return eval(0, nr_token - 1);
}

int check_parentheses(int s, int e) {
/* check if there are illegal brackets,
 * such like ")(", or "()", or "(...))", or "((...)"
 */
	int i, left_bra = 0, flag_in = 0, flag_out = 1;
	for (i = s; i < e + 1; ++ i) {
		if (left_bra < 0) return -1;
		if (flag_in == 1 && left_bra == 0) flag_out = 0;
		if (tokens[i].type == '(') {
			if (tokens[i - 1].type == ')') return -1; 
			else { ++ left_bra; flag_in = 1; }
		}
		if (tokens[i].type == ')') {
			if (tokens[i - 1].type == '(') return -1;
			else { -- left_bra; flag_in = 1; }
		}
	}
	if (left_bra == 0) {
		if (tokens[s].type == '(' && tokens[e].type == ')' && flag_out == 1) 
			return 1;
		else return 0;
	}
	else return -1;
}

int Judge_op(int s, int e) {
/* Priority :
 * *, /
 * +, -
 * ==, !=
 * &&
 * ||
 */
	int i = e;		// Judge "||"
	while (i > s - 1) { 
		if (tokens[i].type == ')') {
			int j = i - 1; 
			while (check_parentheses(j, i) != 1) 
				-- j;
			i = j;
		}
		-- i;
		if (i <= s)
			break;
		if (tokens[i].type == OR) 
			return i;
	}
	i = e;			// Judge "&&"
	while (i > s - 1) {
		if (tokens[i].type == ')') {
			int j = i - 1; 
			while (check_parentheses(j, i) != 1) 
				-- j;
			i = j;
		}
		-- i;
		if (i <= s)
			break;
		if (tokens[i].type == AND) 
			return i;
	}
	i = e;			// Judge "=="
	while (i > s - 1) {
		if (tokens[i].type == ')') {
			int j = i - 1; 
			while (check_parentheses(j, i) != 1) 
				-- j;
			i = j;
		}
		-- i;
		if (i <= s)
			break;
		if (tokens[i].type == EQ) 
			return i;
 	}
 	i = e;			// Judge "!="
 	while (i > s - 1) {
		if (tokens[i].type == ')') {
			int j = i - 1; 
			while (check_parentheses(j, i) != 1) 
				-- j;
			i = j;
 		}
		-- i;
		if (i <= s)
			break;
		if (tokens[i].type == NEQ) 
			return i;
	}
	i = e;			// Judge "+, -"
	while (i > s - 1) {
		if (tokens[i].type == ')') {
			int j = i - 1; 
			while (check_parentheses(j, i) != 1) 
				-- j;
			i = j;
		}
		-- i;
		if (i <= s)
			break;
		bool flag = (tokens[i - 1].type == DIGIT) || (tokens[i - 1].type == ')'); 
		if (tokens[i].type == '+' || (tokens[i].type == '-' && flag)) 
			return i;
	}
	i = e;			// Judge "*, /"
	while (i > s - 1) {
		if (tokens[i].type == ')') {
			int j = i - 1; 
			while (check_parentheses(j, i) != 1) 
				-- j;
			i = j;
		}
		-- i;
		if (i <= s)
			break;
		bool flag = (tokens[i - 1].type == DIGIT) || (tokens[i - 1].type == ')'); 
		if (tokens[i].type == '/' || (tokens[i].type == '*' && flag))
			return i;
	}
	return -1;
}

uint32_t regname(char str[]) {
	if (!strcmp(str, "eax"))
		return cpu.eax;
	if (!strcmp(str, "ecx"))
		return cpu.ecx;
	if (!strcmp(str, "edx"))
		return cpu.edx;
	if (!strcmp(str, "ebx")) 
		return cpu.ebx;
	if (!strcmp(str, "esp"))
		return cpu.esp;
	if (!strcmp(str, "ebp"))
		return cpu.ebp;
	if (!strcmp(str, "esi"))
		return cpu.esi;
	if (!strcmp(str, "edi"))
		return cpu.edi;
	if (!strcmp(str, "eip"))
		return cpu.eip;
	return 0;
}

uint32_t eval(int start, int end) {
	if(start > end) {
		flag = 0;
		return 0;
	}
	else if(start == end) { 
		if (tokens[end].type != DIGIT) {
			if (tokens[end].type == OBJ) {
				return get_addr_obj(tokens[end].str);
			}
			else {
				flag = 0; 
				return 0;
			}
		}
		else {
			if (tokens[end].type == DIGIT && Num_type == 10) {
				uint32_t num = 0;
				int i, len = strlen(tokens[end].str);
			
				for (i = 0; i < len; ++ i) 
					if (!(tokens[end].str[i] >= '0' && tokens[end].str[i] <= '9')) {
						flag = 0;
						return 0;
					}
					else {
						num = num * 10 + tokens[end].str[i] - '0';
					}
			
				return num;
			}
			else if (tokens[end].type == DIGIT && Num_type == 0x10) {
				uint32_t num = 0;
				int i, len = strlen(tokens[end].str);
	
				for (i = 0; i < len; ++ i) 
					if (tokens[end].str[i] >= '0' && tokens[end].str[i] <= '9') 
						num = (num << 4) + tokens[end].str[i] - '0';
					else if (tokens[end].str[i] >= 'A' && tokens[end].str[i] <= 'Z')
						num = (num << 4) + tokens[end].str[i] - 'A' + 10;
					else if (tokens[end].str[i] >= 'a' && tokens[end].str[i] <= 'z')
						num = (num << 4) + tokens[end].str[i] - 'a' + 10;
					else {
						flag = 0;
						assert(0);
					}
	
				Num_type = 10;
				return num;
			}
		}
	}	
	else if (tokens[start].type == HEX) {
		Num_type = 0x10;
		return eval(start + 1, end);
	}
	else {
		int check = check_parentheses(start, end);
	 	if (check == 1) {
			return eval(start + 1, end - 1); 
		}
		else if (check == -1) 
			assert(0);
 		else if (check == 0) {
			int sign = 1;
			int S = start;
			while (tokens[S].type == '-') {
				sign *= -1;
				++ S;
			} 

			int op = Judge_op(start, end);
			if (op == -1) {
				if (tokens[start].type == REG_NAME) {
					if (end - start == 1)
						return regname(tokens[end].str);
					{
						printf("Wrong register!\n");
						assert(0);
					}
				}
				if (tokens[start].type == NOT)					// '!'
					return !eval(start + 1, end);
				else if (tokens[start].type == '-')				// 负数
					return sign * eval(S, end);
				else if (tokens[start].type == '*')
					return swaddr_read(eval(start + 1, end), 4);	// pointer dereference sign
			}
 
			uint32_t val1 = sign * eval(start, op - 1);	
			int op_minus = op;
			sign = 1;
			while (tokens[op_minus + 1].type == '-') {
				sign *= -1;
				op_minus ++;
			}
			uint32_t val2 = sign * eval(op_minus + 1, end);
			// 负数 = sign * abs(number)
 
			switch(tokens[op].type) {
				case '+': return val1 + val2; break;
				case '-': return val1 - val2; break;
				case '*': return val1 * val2; break;
				case '/': return val1 / val2; break;
				case OR: return val1 || val2; break;
				case AND: return val1 && val2; break;
				case EQ: return val1 == val2; break;
				case NEQ: return val1 != val2; break;
				default: assert(0);
			}
		}
 	}
	return 0;
}
