v1-dense:
	g++ -std=c++26 -Wall -Wextra -Werror -pedantic -pedantic-errors -D USE_DENSE_JUMP_TABLE bf_interpreter-v1.cpp -o bf
v1-sparse:
	g++ -std=c++26 -Wall -Wextra -Werror -pedantic -pedantic-errors -D USE_SPARSE_JUMP_TABLE bf_interpreter-v1.cpp -o bf
v2:
	g++ -std=c++26 -Wall -Wextra -Werror -pedantic -pedantic-errors bf_compiler-v2.cpp -o bf
