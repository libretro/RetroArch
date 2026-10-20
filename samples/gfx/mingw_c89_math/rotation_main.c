unsigned rotation_a(unsigned);
unsigned rotation_b(unsigned);
int main(void) { unsigned v; for(v=1;v<=16;v++) { unsigned offset=(v-1)/4, content=(v-1)%4; if(rotation_a(v)!=(content+offset)%4+1 || rotation_b(v)!=offset+1) return 1; } return 0; }
