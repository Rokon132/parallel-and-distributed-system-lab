#include "matrixOp.h"
#include <math.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EPSILON 1e-9

static matrix_result result;
static double result_buffer[MAX_MATRIX_ELEMENTS];
static char message_buffer[ERROR_MESSAGE_LEN];

static void
prepare_result(void)
{
	result.status = 0;
	result.value.rows = 0;
	result.value.cols = 0;
	result.value.data.data_len = 0;
	result.value.data.data_val = result_buffer;
	result.message = message_buffer;
	message_buffer[0] = '\0';
}

static void
set_error(int status, const char *fmt, ...)
{
	va_list args;

	result.status = status;
	result.value.rows = 0;
	result.value.cols = 0;
	result.value.data.data_len = 0;

	va_start(args, fmt);
	vsnprintf(message_buffer, ERROR_MESSAGE_LEN, fmt, args);
	va_end(args);
}

static bool
ensure_valid_matrix(const matrix *m, const char *name)
{
	unsigned long long expected;

	if (m == NULL) {
		set_error(1, "%s is missing", name);
		return false;
	}

	if (m->rows == 0 || m->cols == 0) {
		set_error(1, "%s must have positive dimensions", name);
		return false;
	}

	expected = (unsigned long long)m->rows * (unsigned long long)m->cols;
	if (expected == 0 || expected > MAX_MATRIX_ELEMENTS) {
		set_error(1, "%s exceeds maximum supported elements (%d)", name, MAX_MATRIX_ELEMENTS);
		return false;
	}

	if (m->data.data_len != expected) {
		set_error(1, "%s payload size (%u) does not match %u x %u matrix", name,
			  m->data.data_len, m->rows, m->cols);
		return false;
	}

	if (m->data.data_val == NULL && expected > 0) {
		set_error(1, "%s data buffer is missing", name);
		return false;
	}

	return true;
}

static void
write_success_matrix(u_int rows, u_int cols, u_int elements)
{
	result.status = 0;
	result.value.rows = rows;
	result.value.cols = cols;
	result.value.data.data_len = elements;
	message_buffer[0] = '\0';
}

matrix_result *
matrix_add_1_svc(matrix_pair *argp, struct svc_req *rqstp)
{
	u_int elements;
	const double *a;
	const double *b;

	(void)rqstp;

	prepare_result();

	if (!ensure_valid_matrix(&argp->a, "Matrix A") ||
	    !ensure_valid_matrix(&argp->b, "Matrix B")) {
		return &result;
	}

	if (argp->a.rows != argp->b.rows || argp->a.cols != argp->b.cols) {
		set_error(1, "Matrix dimensions must match for addition");
		return &result;
	}

	elements = argp->a.rows * argp->a.cols;
	a = argp->a.data.data_val;
	b = argp->b.data.data_val;

	for (u_int i = 0; i < elements; ++i) {
		result_buffer[i] = a[i] + b[i];
	}

	write_success_matrix(argp->a.rows, argp->a.cols, elements);
	return &result;
}

matrix_result *
matrix_multiply_1_svc(matrix_pair *argp, struct svc_req *rqstp)
{
	u_int m, n, p;
	u_int elements;
	const double *a;
	const double *b;

	(void)rqstp;

	prepare_result();

	if (!ensure_valid_matrix(&argp->a, "Matrix A") ||
	    !ensure_valid_matrix(&argp->b, "Matrix B")) {
		return &result;
	}

	if (argp->a.cols != argp->b.rows) {
		set_error(1, "Matrix multiplication requires A.cols (%u) == B.rows (%u)",
			  argp->a.cols, argp->b.rows);
		return &result;
	}

	m = argp->a.rows;
	n = argp->a.cols;
	p = argp->b.cols;

	elements = m * p;
	if (elements > MAX_MATRIX_ELEMENTS) {
		set_error(1, "Result exceeds maximum supported elements (%d)", MAX_MATRIX_ELEMENTS);
		return &result;
	}

	a = argp->a.data.data_val;
	b = argp->b.data.data_val;

	for (u_int i = 0; i < m; ++i) {
		for (u_int j = 0; j < p; ++j) {
			double sum = 0.0;
			for (u_int k = 0; k < n; ++k) {
				sum += a[i * n + k] * b[k * p + j];
			}
			result_buffer[i * p + j] = sum;
		}
	}

	write_success_matrix(m, p, elements);

	return &result;
}

matrix_result *
matrix_transpose_1_svc(matrix *argp, struct svc_req *rqstp)
{
	u_int rows, cols;
	const double *input;

	(void)rqstp;

	prepare_result();

	if (!ensure_valid_matrix(argp, "Matrix")) {
		return &result;
	}

	rows = argp->rows;
	cols = argp->cols;
	input = argp->data.data_val;

	for (u_int i = 0; i < rows; ++i) {
		for (u_int j = 0; j < cols; ++j) {
			result_buffer[j * rows + i] = input[i * cols + j];
		}
	}

	write_success_matrix(cols, rows, rows * cols);

	return &result;
}

matrix_result *
matrix_inverse_1_svc(matrix *argp, struct svc_req *rqstp)
{
	u_int n;
	const double *input;
	double *augmented = NULL;
	u_int stride;

	(void)rqstp;

	prepare_result();

	if (!ensure_valid_matrix(argp, "Matrix")) {
		return &result;
	}

	if (argp->rows != argp->cols) {
		set_error(1, "Inverse is defined only for square matrices");
		return &result;
	}

	n = argp->rows;
	input = argp->data.data_val;
	stride = n * 2;

	augmented = malloc(sizeof(double) * n * stride);
	if (augmented == NULL) {
		set_error(2, "Server out of memory while computing inverse");
		return &result;
	}

	for (u_int i = 0; i < n; ++i) {
		for (u_int j = 0; j < n; ++j) {
			augmented[i * stride + j] = input[i * n + j];
			augmented[i * stride + (n + j)] = (i == j) ? 1.0 : 0.0;
		}
	}

	for (u_int col = 0; col < n; ++col) {
		u_int pivot = col;
		double max_val = fabs(augmented[col * stride + col]);

		for (u_int row = col + 1; row < n; ++row) {
			double val = fabs(augmented[row * stride + col]);
			if (val > max_val) {
				max_val = val;
				pivot = row;
			}
		}

		if (max_val < EPSILON) {
			free(augmented);
			set_error(1, "Matrix is singular or near-singular; inverse does not exist");
			return &result;
		}

		if (pivot != col) {
			for (u_int j = 0; j < stride; ++j) {
				double tmp = augmented[col * stride + j];
				augmented[col * stride + j] = augmented[pivot * stride + j];
				augmented[pivot * stride + j] = tmp;
			}
		}

		{
			double pivot_val = augmented[col * stride + col];
			for (u_int j = 0; j < stride; ++j) {
				augmented[col * stride + j] /= pivot_val;
			}
		}

		for (u_int row = 0; row < n; ++row) {
			if (row == col) {
				continue;
			}
			double factor = augmented[row * stride + col];
			if (fabs(factor) < EPSILON) {
				continue;
			}
			for (u_int j = 0; j < stride; ++j) {
				augmented[row * stride + j] -= factor * augmented[col * stride + j];
			}
		}
	}

	for (u_int i = 0; i < n; ++i) {
		for (u_int j = 0; j < n; ++j) {
			result_buffer[i * n + j] = augmented[i * stride + (n + j)];
		}
	}

	write_success_matrix(n, n, n * n);

	free(augmented);

	return &result;
}
