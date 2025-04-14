// Console input and output.
// Input is from the keyboard or serial port.
// Output is written to the screen and serial port.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include <stddef.h>
#define COPY_BUF_SIZE 128
#define LEFT_ARROW 0xE04B 
#define COPY_BUF_SIZE 128
#define INPUT_BUF 128
char* BUF[INPUT_BUF];
 int selecting = 0;
 char copy_buffer[COPY_BUF_SIZE];    
 int copy_index = 0;                               
#define CMD_HISTORY 10      
#define CMD_MAX_LEN 128 
static void consputc(int);
char cmd_history[CMD_HISTORY][CMD_MAX_LEN];  
int cmd_history_index = 0;                      
int cmd_count = 0; 
#define HIST_SIZE 10
struct hist{
  uint queue_idx;
  char cmd_buf[HIST_SIZE][INPUT_BUF];
  uint last_used_idx;

  int is_suggestion_used;
  char original_cmd[INPUT_BUF];
  uint original_cmd_size;

  int total_count;
} hist; 
#define INPUT_BUF 128
struct {
  char buf[INPUT_BUF];
  uint r;  // Read index
  uint w;  // Write index
  uint e;  // Edit index
} input;
static void
push_current_hist()
{
  if(input.e - input.w == 1)
    return;
  memset(hist.cmd_buf[hist.queue_idx], 0, INPUT_BUF);
  memmove(hist.cmd_buf[hist.queue_idx],
          input.buf + input.w,
          input.e - input.w - 1);
  hist.queue_idx = (hist.queue_idx + 1) % HIST_SIZE;
  hist.total_count++;
  hist.is_suggestion_used = 0;
  hist.last_used_idx = 0;
  memset(hist.original_cmd, 0, INPUT_BUF);
}                         
int strncmp_c(const char *str1, const char *str2, size_t n) {
    while (n-- > 0) {
        if (*str1 != *str2) {
            return (unsigned char)*str1 - (unsigned char)*str2;
        }
        if (*str1 == '\0') {
            break;            
        }
        str1++;
        str2++;
    }
    return 0;  
}


char* strcpy_c(char* dest, const char* src) {
    char* dest_ptr = dest;
    
    while (*src) {
        *dest_ptr = *src;
        dest_ptr++;
        src++;
    }
    
    *dest_ptr = '\0';  
    return dest;   
}

void add_to_history(char *cmd) {
    if (strlen(cmd) == 0) return;
    strncpy(cmd_history[cmd_history_index], cmd, CMD_MAX_LEN);
    cmd_history_index = (cmd_history_index + 1) % CMD_HISTORY;
    if (cmd_count < CMD_HISTORY) cmd_count++;
}


void show_history() {
    int start = (cmd_history_index - cmd_count + CMD_HISTORY) % CMD_HISTORY;
    for (int i = 0; i < cmd_count; i++) {
        int index = (start + i) % CMD_HISTORY;
        char *cmd = cmd_history[index];
        while (*cmd) {
            consputc(*cmd);
            cmd++;
        }
        //consputc('\n');
    }
}

char* autocomplete(char *input) {
    int match_count = 0;
    char* match = NULL;
    for (int i = 0; i < cmd_count; i++) {
        //int i = (cmd_history_index - i - 1 + CMD_HISTORY) % CMD_HISTORY;
        if (strncmp_c(cmd_history[i], input, strlen(input)) == 0) {
            if (match_count == 0) {
                match = cmd_history[i];  
            }
            match_count++;
        }
    }

    if (match_count == 1) {
        return match; 
    } else {
        return NULL;
    }
}



static int panicked = 0;

static struct {
  struct spinlock lock;
  int locking;
} cons;

static void
printint(int xx, int base, int sign)
{
  static char digits[] = "0123456789abcdef";
  char buf[16];
  int i;
  uint x;

  if(sign && (sign = xx < 0))
    x = -xx;
  else
    x = xx;

  i = 0;
  do{
    buf[i++] = digits[x % base];
  }while((x /= base) != 0);

  if(sign)
    buf[i++] = '-';

  while(--i >= 0)
    consputc(buf[i]);
}
//PAGEBREAK: 50

// Print to the console. only understands %d, %x, %p, %s.
void
cprintf(char *fmt, ...)
{
  int i, c, locking;
  uint *argp;
  char *s;

  locking = cons.locking;
  if(locking)
    acquire(&cons.lock);

  if (fmt == 0)
    panic("null fmt");

  argp = (uint*)(void*)(&fmt + 1);
  for(i = 0; (c = fmt[i] & 0xff) != 0; i++){
    if(c != '%'){
      consputc(c);
      continue;
    }
    c = fmt[++i] & 0xff;
    if(c == 0)
      break;
    switch(c){
    case 'd':
      printint(*argp++, 10, 1);
      break;
    case 'x':
    case 'p':
      printint(*argp++, 16, 0);
      break;
    case 's':
      if((s = (char*)*argp++) == 0)
        s = "(null)";
      for(; *s; s++)
        consputc(*s);
      break;
    case '%':
      consputc('%');
      break;
    default:
      // Print unknown % sequence to draw attention.
      consputc('%');
      consputc(c);
      break;
    }
  }

  if(locking)
    release(&cons.lock);
}

void
panic(char *s)
{
  int i;
  uint pcs[10];

  cli();
  cons.locking = 0;
  // use lapiccpunum so that we can call panic from mycpu()
  cprintf("lapicid %d: panic: ", lapicid());
  cprintf(s);
  cprintf("\n");
  getcallerpcs(&s, pcs);
  for(i=0; i<10; i++)
    cprintf(" %p", pcs[i]);
  panicked = 1; // freeze other CPU
  for(;;)
    ;
}

//PAGEBREAK: 50
#define BACKSPACE 0x100
#define CRTPORT 0x3d4
static ushort *crt = (ushort*)P2V(0xb8000);  // CGA memory

static void
cgaputc(int c)
{
  int pos;

  // Cursor position: col + 80*row.
  outb(CRTPORT, 14);
  pos = inb(CRTPORT+1) << 8;
  outb(CRTPORT, 15);
  pos |= inb(CRTPORT+1);

  if(c == '\n')
    pos += 80 - pos%80;
  else if(c == BACKSPACE){
    if(pos > 0) --pos;
  } else
    crt[pos++] = (c&0xff) | 0x0700;  // black on white

  if(pos < 0 || pos > 25*80)
    panic("pos under/overflow");

  if((pos/80) >= 24){  // Scroll up.
    memmove(crt, crt+80, sizeof(crt[0])*23*80);
    pos -= 80;
    memset(crt+pos, 0, sizeof(crt[0])*(24*80 - pos));
  }

  outb(CRTPORT, 14);
  outb(CRTPORT+1, pos>>8);
  outb(CRTPORT, 15);
  outb(CRTPORT+1, pos);
  crt[pos] = ' ' | 0x0700;
}

void
consputc(int c)
{
  if(panicked){
    cli();
    for(;;)
      ;
  }

  if(c == BACKSPACE){
    uartputc('\b'); uartputc(' '); uartputc('\b');
  } else
    uartputc(c);
  cgaputc(c);
}



#define C(x)  ((x)-'@')  // Control-x
void backwardCursor() {
    if (input.r > 0) {
        input.r--;
        consputc(BACKSPACE);
    }
}
static int
get_suggestion(const char* cmd, uint cmd_size)
{
  for(int i = 0; i < HIST_SIZE; ++i){
    int idx = (i + hist.last_used_idx) % HIST_SIZE;
    if(strncmp(cmd, hist.cmd_buf[idx], cmd_size) == 0){
      return idx;
    }
  }
  return -1;
}
void
consputs(const char* s){
  for(int i = 0; i < INPUT_BUF && (s[i]); ++i){
    input.buf[input.e++ % INPUT_BUF] = s[i];
    consputc(s[i]);
  }
}

void
consclear(){
  while(input.e != input.w &&
        input.buf[(input.e-1) % INPUT_BUF] != '\n'){
    input.e--;
    consputc(BACKSPACE);
  }
}
static void
suggest_cmd()
{
  if(!hist.is_suggestion_used){
    hist.original_cmd_size = input.e - input.w;
    memmove(hist.original_cmd, input.buf + input.w, hist.original_cmd_size);
  }
  int suggested_cmd = get_suggestion(hist.original_cmd, hist.original_cmd_size);
  if(suggested_cmd >= 0){
    hist.is_suggestion_used = 1;
    hist.last_used_idx = suggested_cmd + 1;
    consclear();
    consputs(hist.cmd_buf[suggested_cmd]);
  }
  else // beep
    consputc('\a');
}
void
consoleintr(int (*getc)(void))
{
  int c, doprocdump = 0;
  int i,j;
  int sec_pos,first_pos;
  int last_size;
  acquire(&cons.lock);
  while((c = getc()) >= 0){
    switch(c){
    case C('P'):  // Process listing.
      // procdump() locks cons.lock indirectly; invoke later
      doprocdump = 1;
      break;
    case C('U'):  // Kill line.
      while(input.e != input.w &&
            input.buf[(input.e-1) % INPUT_BUF] != '\n'){
        input.e--;
        consputc(BACKSPACE);
      }
      break;
    
    case C('C'):
      if (selecting == 0){
      first_pos =input.e ; 
      }
      else if(selecting==1){
        sec_pos=input.e;
        //memset(copy_buf,0,K);
        for (j = sec_pos; j < first_pos; j++){
        copy_buffer[j-first_pos]= input.buf[j%INPUT_BUF];
        }
          last_size = first_pos - sec_pos;
      }
    case C('V'):
      for(j = 0; j < last_size; j++)
      {
        input.buf[input.e++ % INPUT_BUF]= copy_buffer[j];
        consputc(copy_buffer[j]);
      }
      break;
    case '\t':
    	suggest_cmd();
      break;

     break;
    case C('H'):
       show_history();
      break;
    
    case '\x7f':  // Backspace
      if(input.e != input.w){
        input.e--;
        consputc(BACKSPACE);
      }
      break;
    
    default:
      if(c != 0 && input.e-input.r < INPUT_BUF){
        c = (c == '\r') ? '\n' : c;
        input.buf[input.e++ % INPUT_BUF] = c;
        consputc(c);
        if(c == '\n' || c == C('D') || input.e == input.r+INPUT_BUF){
          input.buf[input.e % INPUT_BUF] = '\0'; 
          add_to_history(input.buf + input.w);
          push_current_hist();
          input.w = input.e;
          wakeup(&input.r);
        }
      }
      break;
    }
  }
  release(&cons.lock);
  if(doprocdump) {
    procdump();  // now call procdump() wo. cons.lock held
  }
}

int
consoleread(struct inode *ip, char *dst, int n)
{
  uint target;
  int c;

  iunlock(ip);
  target = n;
  acquire(&cons.lock);
  while(n > 0){
    while(input.r == input.w){
      if(myproc()->killed){
        release(&cons.lock);
        ilock(ip);
        return -1;
      }
      sleep(&input.r, &cons.lock);
    }
    c = input.buf[input.r++ % INPUT_BUF];
    if(c == C('D')){  // EOF
      if(n < target){
        // Save ^D for next time, to make sure
        // caller gets a 0-byte result.
        input.r--;
      }
      break;
    }
    *dst++ = c;
    --n;
    if(c == '\n')
      break;
  }
  release(&cons.lock);
  ilock(ip);

  return target - n;
}

int
consolewrite(struct inode *ip, char *buf, int n)
{
  int i;

  iunlock(ip);
  acquire(&cons.lock);
  for(i = 0; i < n; i++)
    consputc(buf[i] & 0xff);
  release(&cons.lock);
  ilock(ip);

  return n;
}

void
consoleinit(void)
{
  initlock(&cons.lock, "console");

  devsw[CONSOLE].write = consolewrite;
  devsw[CONSOLE].read = consoleread;
  cons.locking = 1;

  ioapicenable(IRQ_KBD, 0);
}
