.PHONY: pow2 zero_evens

zero_evens:
	$(CC) zero_evens.c -o zero_evens
	objdump -M intel -M hex --no-show-raw-insn -d zero_evens > zero_evens.objdump

zero_evens_opt:
	$(CC) -O3 zero_evens.c -o zero_evens_opt
	objdump -M intel -M hex --no-show-raw-insn -d zero_evens_opt > zero_evens_opt.objdump
