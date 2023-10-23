int dump2(int pid, int register_num, uint64 *return_value){

	const int reg_min = 2;
	const int reg_max = 11;
	const int reg_bytes_len = 8;

	if(register_num < reg_min || register_num > reg_max) return -3;

	struct proc *my_proc = myproc();
	int mid = my_proc->pid;
	

  	for (struct proc* it = proc; it < &proc[NPROC]; it++){
    		proc->lock.locked = 1;
    		if(it->pid == pid){
      			if((it->pid != mid) && (!it->parent || (it->parent && it->parent->pid != mid))){ 
        			proc->lock.locked = 0;
        			return -1;
     		 	}
      		
      			uint64 *reg_ptr = &(it->trapframe->s2);
			reg_ptr += register_num - reg_min;
			
			int written = copyout(my_proc->pagetable, *return_value, (char*) reg_ptr, reg_bytes_len);
			proc->lock.locked = 0;
			if(written) return -4;
     		 	else return 0;

    		}
    		proc->lock.locked = 0;
  	}

    	return -2;	
}

