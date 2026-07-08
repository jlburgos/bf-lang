v1-dense:
	g++ -std=c++26 -Wall -Wextra -Werror -pedantic -pedantic-errors -D USE_DENSE_JUMP_TABLE v1-interpreter/bf_interpreter.cpp -o bfc
v1-sparse:
	g++ -std=c++26 -Wall -Wextra -Werror -pedantic -pedantic-errors -D USE_SPARSE_JUMP_TABLE v1-interpreter/bf_interpreter.cpp -o bfc
v2-debug:
	g++ -std=c++26 -Wall -Wextra -Werror -pedantic -pedantic-errors -DDEBUG -g v2-compiler/bf_compiler.cpp -o bfc
v2:
	g++ -std=c++26 -Wall -Wextra -Werror -pedantic -pedantic-errors v2-compiler/bf_compiler.cpp -o bfc
