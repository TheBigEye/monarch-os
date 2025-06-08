#include "arithmetic.h"

#include "../memory/memory.h"

/**
 * Decode and calculate the sum, subtraction, product, or division of two or more numbers.
 *
 * @param input The input string.
 * @param position The starting position in the input.
 * @return The result of the operation on the numbers (as a float).
 */
double getMath(char input[], int position) {
    double result = 0.0;
    double number = 0.0;
    char operation = '+';
    double decimal = 0.1;
    bool isfloat = false;

    for (int i = position; i < strlen(input); i++) {
        if (input[i] >= '0' && input[i] <= '9') {
            if (isfloat) {
                number += (input[i] - '0') * decimal;
                decimal *= 0.1;
            } else {
                number *= 10.0;
                number += (input[i] - '0');
            }
        } else if (input[i] == '.' || input[i] == ',') {
            isfloat = true;
        } else if (input[i] == '+' || input[i] == '-' || input[i] == '*' || input[i] == '/') {
            switch (operation) {
                case '+': result += number; break;
                case '-': result -= number; break;
                case '*': result *= number; break;
                case '/': result = ABS(number) > EPSILON ? result / number : 0.0; break;
                default: break;
            }
            number = 0.0;
            operation = input[i];
            isfloat = false;
            decimal = 0.1;
        }
    }

    switch (operation) {
        case '+': result += number; break;
        case '-': result -= number; break;
        case '*': result *= number; break;
        case '/': result = ABS(number) > EPSILON ? result / number : 0.0; break;
        default: break;
    }

    return result;
}
