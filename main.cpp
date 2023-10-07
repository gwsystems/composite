#include <stdio.h>

int functioncall(int i, int sum) {
    for(i = 0 ; i <= 10 ; ++i) {
        sum += 456;
    }
    sum = sum - 55;
    return sum;
}

int foocall(int i, int sum) {
    int* array;
    array = new int[10];
    for(int i = 0 ; i <= 10 ; ++i) {
        sum += 456;
        array[i] = sum;
    }
    sum = array[5]- array[7];
    return sum;
}

int fib(int n) {
    if (n == 1) {
        return 1;
    } else if (n == 2) {
        return 1;
    }
    return fib(n-1) + fib(n-2);
}


int main(int argc, char* argv[]) {
    int i = 1;
    int sum = 0;
    sum = functioncall(i, sum);
    sum = foocall(i, sum);
    return fib(20);
}