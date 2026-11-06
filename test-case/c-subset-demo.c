// 运行时加 --c-subset，以启用 C 的单字符 &、| 位运算。
int classify(int);
int total = 0;
void ping(void);

int main(void) {
    int a[][3] = {{1, 2, 3}, {4, 5, 6},};
    int i = 0, j = 5;

    i++;
    ++i;
    j--;
    --j;
    total += i;
    total -= j;
    total *= 2;
    total /= 2;
    total %= 100;
    total <<= 1;
    total >>= 1;
    total &= 0x7f;
    total |= 02;
    total ^= 1;

    for (int row = 0; row < 2; ++row) {
        for (int col = 0; col < 3; col++) {
            total += a[row][col];
        }
    }
    for (i = 0, j = 5; i < j && total != 0; ++i, --j) {
        if (i == 1) continue;
        total += i;
    }
    for (;;) {
        break;
    }
    while (i > 0) {
        --i;
    }
    do {
        i += 1;
    } while (i < 3);

    total = i ? total : j ? 10 : 20;
    total = (total & 7) | (i ^ j);
    a[i = 0][j = 1]++;
    total = (i++, j++, a[0][1]);
    total = sizeof(int) + sizeof a[0];

    switch (classify(total)) {
        case -1: total--; break;
        case 1 + 1: { int value = 2; total += value; } break;
        default: total = 0;
    }
    printf("%d %d\n", total, classify(i++));
    ping();
    return total > 0 ? 0 : 1;
}

int classify(int x) {
    return x < 0 ? -1 : x == 0 ? 0 : 2;
}

void ping(void) {
    return;
}
