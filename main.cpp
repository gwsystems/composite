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


int main(int argc, char* argv[]) {
    int i = 1;
    int sum = 0;
    sum = functioncall(i, sum);
    sum = foocall(i, sum);
}