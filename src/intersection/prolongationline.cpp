/*
 * ProlongationLine.cpp
 *
 *  Created on: 2.12.2014
 *      Author: viktor
 */

#include "prolongationline.h"

using namespace std;
namespace computeintersection{

ProlongationLine::ProlongationLine(unsigned int element_2D, unsigned int dictionary){
	elm_2D_idx = element_2D;
	dictionary_idx = dictionary;
}

ProlongationLine::ProlongationLine(unsigned int side_old,unsigned int side,unsigned int element_old, unsigned int element,unsigned int type) {

	side_idx = side;
	side_idx_old = side_old;

	element_idx = element;
	element_idx_old = element_old;

	if(type == 0){
		// 2D prolongation
		pluckerProducts.reserve(6);
		pluckers.reserve(1);

	}else if(type == 1){
		// 3D prolongation
		pluckerProducts.reserve(9);
		pluckers.reserve(6);
	}else{
		//xprintf(Msg,"\n\nUndefined prolongation type!\n\n");
		//throw std::exception("Undefined prolongation type");
	}
}

}

