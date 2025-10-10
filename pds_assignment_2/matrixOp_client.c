#include "matrixOp.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

static void
discard_line(void)
{
	int ch;

	while ((ch = getchar()) != '\n' && ch != EOF) {
		/* discard extraneous characters on the current line */
	}
}

static bool
read_matrix(const char *label, matrix *out)
{
	u_int rows = 0;
	u_int cols = 0;
	unsigned long long total;
	double *data = NULL;

	out->rows = 0;
	out->cols = 0;
	out->data.data_len = 0;
	out->data.data_val = NULL;

	printf("Enter rows and columns for matrix %s (rows cols): ", label);
	if (scanf("%u %u", &rows, &cols) != 2) {
		fprintf(stderr, "Invalid dimensions. Please enter positive integers.\n");
		discard_line();
		return false;
	}

	if (rows == 0 || cols == 0) {
		fprintf(stderr, "Dimensions must be positive integers.\n");
		return false;
	}

	total = (unsigned long long)rows * (unsigned long long)cols;
	if (total == 0 || total > MAX_MATRIX_ELEMENTS) {
		fprintf(stderr, "Matrix too large. Maximum supported elements: %d\n", MAX_MATRIX_ELEMENTS);
		return false;
	}

	data = malloc(sizeof(double) * total);
	if (data == NULL) {
		fprintf(stderr, "Unable to allocate memory for matrix values.\n");
		return false;
	}

	printf("Enter %llu values for matrix %s (row-major order):\n", total, label);
	for (unsigned long long i = 0; i < total; ++i) {
		if (scanf("%lf", &data[i]) != 1) {
			fprintf(stderr, "Invalid numeric input encountered.\n");
			free(data);
			discard_line();
			return false;
		}
	}

	out->rows = rows;
	out->cols = cols;
	out->data.data_len = (u_int)total;
	out->data.data_val = data;
	return true;
}

static void
free_matrix(matrix *m)
{
	if (m->data.data_val != NULL) {
		free(m->data.data_val);
		m->data.data_val = NULL;
	}
	m->data.data_len = 0;
	m->rows = 0;
	m->cols = 0;
}

static void
print_matrix(const matrix *m)
{
	for (u_int i = 0; i < m->rows; ++i) {
		for (u_int j = 0; j < m->cols; ++j) {
			printf("%10.4f ", m->data.data_val[i * m->cols + j]);
		}
		printf("\n");
	}
}

static void
print_result(const char *operation, const matrix_result *res)
{
	if (res == NULL) {
		printf("%s failed: unable to reach server.\n", operation);
		return;
	}

	if (res->status != 0) {
		printf("%s failed: %s\n", operation, res->message != NULL ? res->message : "unknown error");
		return;
	}

	printf("%s result (%u x %u):\n", operation, res->value.rows, res->value.cols);
	print_matrix(&res->value);
}

static void
interactive_loop(CLIENT *clnt)
{
	bool running = true;

	while (running) {
		int choice;

		printf("\n=== Matrix Operator Menu ===\n");
		printf("1) Matrix Addition (A + B)\n");
		printf("2) Matrix Multiplication (A x B)\n");
		printf("3) Matrix Transpose (A^T)\n");
		printf("4) Matrix Inverse (A^-1)\n");
		printf("0) Exit\n");
		printf("Select an option: ");

		if (scanf("%d", &choice) != 1) {
			fprintf(stderr, "Invalid selection. Please enter a number between 0 and 4.\n");
			discard_line();
			continue;
		}

		switch (choice) {
		case 0:
			running = false;
			break;
		case 1:
		{
			matrix a;
			matrix b;
			matrix_pair pair;
			matrix_result *res;

			if (!read_matrix("A", &a)) {
				break;
			}
			if (!read_matrix("B", &b)) {
				free_matrix(&a);
				break;
			}

			pair.a = a;
			pair.b = b;
			res = matrix_add_1(&pair, clnt);
			if (res == NULL) {
				clnt_perror(clnt, "matrix_add");
			}
			print_result("Addition", res);
			free_matrix(&a);
			free_matrix(&b);
			break;
		}
		case 2:
		{
			matrix a;
			matrix b;
			matrix_pair pair;
			matrix_result *res;

			if (!read_matrix("A", &a)) {
				break;
			}
			if (!read_matrix("B", &b)) {
				free_matrix(&a);
				break;
			}

			pair.a = a;
			pair.b = b;
			res = matrix_multiply_1(&pair, clnt);
			if (res == NULL) {
				clnt_perror(clnt, "matrix_multiply");
			}
			print_result("Multiplication", res);
			free_matrix(&a);
			free_matrix(&b);
			break;
		}
		case 3:
		{
			matrix input;
			matrix_result *res;

			if (!read_matrix("A", &input)) {
				break;
			}

			res = matrix_transpose_1(&input, clnt);
			if (res == NULL) {
				clnt_perror(clnt, "matrix_transpose");
			}
			print_result("Transpose", res);
			free_matrix(&input);
			break;
		}
		case 4:
		{
			matrix input;
			matrix_result *res;

			if (!read_matrix("A", &input)) {
				break;
			}

			res = matrix_inverse_1(&input, clnt);
			if (res == NULL) {
				clnt_perror(clnt, "matrix_inverse");
			}
			print_result("Inverse", res);
			free_matrix(&input);
			break;
		}
		default:
			printf("Unknown option %d. Please select between 0 and 4.\n", choice);
			break;
		}
	}
}


void
matrix_op_prog_1(char *host)
{
	CLIENT *clnt;

#ifndef	DEBUG
	clnt = clnt_create(host, MATRIX_OP_PROG, MATRIX_OP_V1, "tcp");
	if (clnt == NULL) {
		clnt = clnt_create(host, MATRIX_OP_PROG, MATRIX_OP_V1, "udp");
	}
	if (clnt == NULL) {
		clnt_pcreateerror(host);
		exit(1);
	}
#endif	/* DEBUG */

	interactive_loop(clnt);

#ifndef	DEBUG
	clnt_destroy(clnt);
#endif	 /* DEBUG */
}


int
main (int argc, char *argv[])
{
	if (argc != 2) {
		printf("Usage: %s <server_host>\n", argv[0]);
		exit(1);
	}

	matrix_op_prog_1(argv[1]);
	return 0;
}
