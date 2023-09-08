.global main
main:

// TODO: [p0-task1] Add your code here!

	li s0, 1  // i = 1
	li s1, 101 // n = 101
	bge s0, s1, done // if i >= n, done   0x50000008

loop:
	mv a0, s0
	call is_prime
	addi s0, s0, 1 // ++i
	blt s0, s1, loop

done:
end:
	j end

is_prime:
	li a1, 1
	beq a0, a1, is_not_prime
	li t0, 2
	bge t0, a0, is_prime_done

is_prime_loop:
	rem t1, a0, t0
	beqz t1, is_not_prime
	addi t0, t0, 1
	blt t0, a0, is_prime_loop
	j is_prime_done

is_not_prime:
	li a1, 0
	j is_prime_done

is_prime_done:
	ret
