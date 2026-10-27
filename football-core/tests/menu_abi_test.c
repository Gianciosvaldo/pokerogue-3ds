#include <stdio.h>
#include <assert.h>
/* FootballMenuRow is built by football_live_screen.c and read by football_menu.c. The 3DS compiler makes an enum as small as it can be
 * (-fshort-enums), and text_style is such an enum in one file and an int in the other (ui_text.h and mui.h each define it, whichever is
 * included first wins), so a struct holding a text_style is laid out differently in the two: on the console the draft list read the
 * label of its fourth row from the third row's style (0x1D) and crashed. The host's enums are int-sized, which hid it. This builds the
 * struct as each of the two files sees it, from the files' own include lines, with the console's enum size. */
unsigned abi_screen(unsigned *dim,unsigned *indent);
unsigned abi_menu(unsigned *dim,unsigned *indent);
int main(void)
{
    unsigned dim_a,indent_a,dim_b,indent_b;
    const unsigned a=abi_screen(&dim_a,&indent_a),b=abi_menu(&dim_b,&indent_b);
    if(a!=b || dim_a!=dim_b || indent_a!=indent_b)fprintf(stderr,"FootballMenuRow differs: the screen sees %u bytes (dim at %u, indent at %u), the menu %u (%u, %u)\n",a,dim_a,indent_a,b,dim_b,indent_b);
    assert(a==b && dim_a==dim_b && indent_a==indent_b);
    printf("football menu ABI: FootballMenuRow is %u bytes in both files under -fshort-enums; passed\n",a);
    return 0;
}
