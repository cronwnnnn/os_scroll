[global atomic_exchange]
[global compare_and_exchange]

atomic_exchange:
    ; ecx为第一个参数，是要修改的锁
    ; eax是修改的值，为1，当eax返回时如果为1表示修改不成功，因为锁的值当前为1
    ; 当返回0时表示成功，将锁的值由0变1了
    mov ecx, [esp+4]
    mov eax, [esp+8]
    xchg [ecx], eax
    ret

compare_and_exchange:
    mov edx, [esp + 4]
    mov ecx, [esp + 8]
    mov eax, 0
    ; edx为锁的地址，ecx为上锁的值1
    ; 比较第一个操作数和 eax 的值，如果相同，即lock是0，则将ecx的值写入edx中，即将lock置为1，eax返回0表示成功上锁
    ; 如果不相同，表明lock当前为1，就会将第一个操作数的值，赋值给 eax，即将返回值设为 1，表示未能上锁
    lock cmpxchg [edx], ecx
    ret
