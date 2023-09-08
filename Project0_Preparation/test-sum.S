.global main
main:

// TODO: [p0-task1] Add your code here!

	li a1, 0  // sum = 0
	li s0, 1  // i = 1
	li s1, 101 // n = 101
	bge s0, s1, done // if i >= n, done   0x50000008

loop:
	add a1, a1, s0
	addi s0, s0, 1 // ++i
	blt s0, s1, loop

done:
end:
	j end
