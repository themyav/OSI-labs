int dump(void){
	struct proc *p = myproc();
	uint64 *s2_ptr = &(p->trapframe->s2);
	for(int i = 0; i < 10; i++){
		 printf("s%d = %d\n", i + 2, (uint32) *(s2_ptr + i));
	}
	return 0;
}
