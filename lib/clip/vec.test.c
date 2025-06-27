#import test
#import clip/vec

int main() {
    vec.t *l = vec.new(sizeof(int));
    for (int i = 0; i < 10000; i++) {
		vec.push(l, &i);
    }
    for (int i = 0; i < 10000; i++) {
		int *x = vec.index(l, i);
        test.truth("eq", i == *x);
    }
    vec.free(l);
    return test.fails();
}
