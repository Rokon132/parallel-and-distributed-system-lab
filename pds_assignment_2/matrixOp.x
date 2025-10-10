const MAX_MATRIX_ELEMENTS = 400;
const ERROR_MESSAGE_LEN = 256;

struct matrix {
    u_int rows;
    u_int cols;
    double data<MAX_MATRIX_ELEMENTS>;
};

struct matrix_pair {
    matrix a;
    matrix b;
};

struct matrix_result {
    int status; /* 0 = success, non-zero = error */
    matrix value;
    string message<ERROR_MESSAGE_LEN>;
};

program MATRIX_OP_PROG {
    version MATRIX_OP_V1 {
        matrix_result MATRIX_ADD(matrix_pair) = 1;
        matrix_result MATRIX_MULTIPLY(matrix_pair) = 2;
        matrix_result MATRIX_TRANSPOSE(matrix) = 3;
        matrix_result MATRIX_INVERSE(matrix) = 4;
    } = 1;
} = 0x31234567;
