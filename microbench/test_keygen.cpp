#include <iostream>
#include <bits/stdc++.h> 
#include <time.h>
#include <stdlib.h>
#include "random_fnv1a.h"
#include "keygen.h"

using namespace std;

template<class keygen_t>
void get_random_numbers(int *arr, int numKeys, keygen_t keygen)
{
	for (int i=0; i < numKeys; i++) {
		arr[i] = keygen.next();
	}
	cout << endl;
}

void print_random_numbers(int *arr, int numKeys)
{
	for (int i=0; i < numKeys; i++) {
		cout << " " << arr[i] << flush;
	}
	cout << endl;
}

int *dists;

void calculate_and_print_distribution(int *arr, const int numKeys, const int maxKey)
{
	for (int i=0; i <= maxKey; i++) dists[i] = 0;
	for (int i=0; i < numKeys; i++) dists[arr[i]]++;

	for (int i=0; i <= 30; i++) {
		double prob = double(dists[i]) / numKeys * 100.0;
		cout << i << ": " << dists[i] << " (" << prob << "%)" << endl;
	}
}

int main(int argc, char *argv[])
{
	const int maxKey = 1000000;
	const int numKeys = 50000000;
	int *keys;
	int seed = time(NULL);
	double theta = atof(argv[1]);

	keys = (int *)malloc(numKeys * sizeof(int));
	dists = (int *)malloc((maxKey+1) * sizeof(int));
	if (!keys || !dists) {
		cout << "Could not allocate memory for keys and distributions" << endl;
		return 1;
	}

	KeyGeneratorUniform<int> uni(new RandomFNV1A(seed), maxKey);
	KeyGeneratorZipf<int> zipf(new KeyGeneratorZipfData(maxKey, theta), new RandomFNV1A(seed));

//	cout << "Uniform: " << flush;
//	get_random_numbers(keys, numKeys, uni);
////	print_random_numbers(keys, numKeys);
//	calculate_and_print_distribution(keys, numKeys, maxKey);

	cout << "Zipf: " << flush;
	get_random_numbers(keys, numKeys, zipf);
//	print_random_numbers(keys, numKeys);
	calculate_and_print_distribution(keys, numKeys, maxKey);

	return 0;
}
