#include <iostream>
#include "MakeAtmFlux.h"

using namespace std;
int main(void)
{
	cout << "hello" << endl;


	string cardFile = "../resources/honda-2015-spl-solmin.d";
	MakeAtmFlux * flux = new MakeAtmFlux(cardFile);
	cout << flux->InterpolateFlux(1, 10000000, 0.97, 0, 2) << endl;
}