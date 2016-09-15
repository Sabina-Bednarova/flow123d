/*!
 *
﻿ * Copyright (C) 2015 Technical University of Liberec.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 3 as published by the
 * Free Software Foundation. (http://www.gnu.org/licenses/gpl-3.0.en.html)
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 *
 * @file    unit_converter.cc
 * @brief
 */


#include "fields/unit_converter_template.hh"


/*******************************************************************
 * implementation of BasicFactors
 */

BasicFactors::BasicFactors() {
	units_map_ = {
			{ "*m",  { 1,           UnitSI().m() } },
			{ "*g",  { 0.001,       UnitSI().kg() } },
			{ "*s",  { 1,           UnitSI().s() } },
			{ "*A",  { 1,           UnitSI().A() } },
			{ "*K",  { 1,           UnitSI().K() } },
			{ "*cd", { 1,           UnitSI().cd() } },
			{ "*mol",{ 1,           UnitSI().mol() } },

			{ "*N",  { 1,           UnitSI().m().kg().s(-2) } },
			{ "*J",  { 1,           UnitSI().m(2).kg().s(-2) } },
			{ "*W",  { 1,           UnitSI().m(2).kg().s(-3) } },
			{ "*Pa", { 1,           UnitSI().m(-1).kg().s(-2) } },

			{ "cm",  { 0.01,        UnitSI().m() } },
			{ "dm",  { 0.1,         UnitSI().m() } },
			{ "t",   { 1000,        UnitSI().kg() } },
			{ "min", { 60,          UnitSI().s() } },
			{ "h",   { 3600,        UnitSI().s() } },
			{ "d",   { 24*3600,     UnitSI().s() } },
			{ "y",   { 365*24*3600, UnitSI().s() } },
			{ "hPa", { 100,         UnitSI().m(-1).kg().s(-2) } },

			{ "rad", { 1,           UnitSI().m(0) } }
	};

	// map of prefixes and multiplicative constants
	std::map<std::string, double> prefix_map = {
			{ "p", 1e-12 },
			{ "n", 1e-9 },
			{ "u", 1e-6 },
			{ "m", 1e-3 },
			{ "",  1 },
			{ "k", 1e+3 },
			{ "M", 1e+6 },
			{ "G", 1e+9 },
			{ "T", 1e+12 }
	};

	// add derived units
	std::map<std::string, DerivedUnit>::iterator it;
	std::vector<std::string> delete_keys; // store keys start with '*' (will be deleted)
	for (it=units_map_.begin(); it!=units_map_.end(); ++it) {
		if (it->first.at(0)=='*') {
			std::string shortcut = it->first.substr(1);
			double coef = it->second.coef_;

			for (std::map<std::string, double>::iterator prefix_it=prefix_map.begin(); prefix_it!=prefix_map.end(); ++prefix_it) {
				std::string key = prefix_it->first + shortcut;
				units_map_.insert(std::pair<std::string, DerivedUnit>( key, { coef*prefix_it->second, it->second.unit_ } ));
			}

			delete_keys.push_back(it->first);
		}
	}

	// delete keys start with '*'
	for (std::vector<std::string>::iterator del_it = delete_keys.begin(); del_it!=delete_keys.end(); ++del_it) {
		it = units_map_.find(*del_it);
		units_map_.erase(it);
	}
}


/*******************************************************************
 * implementation of UnitConverter
 */

UnitConverter::UnitConverter()
: coef_(1.0) {}


const BasicFactors UnitConverter::basic_factors = BasicFactors();


UnitData UnitConverter::read_unit(std::string s)
{
    typedef spirit_namespace::position_iterator< std::string::iterator > PosnIterT;

    std::string::iterator begin = s.begin();
	std::string::iterator end = s.end();

    const PosnIterT posn_begin( begin, end );
    const PosnIterT posn_end( end, end );

    units_converter::Semantic_actions< std::string::iterator > semantic_actions;

	try {
		spirit_namespace::parse( begin, end,
							units_converter::UnitSIGrammer< std::string::iterator >( semantic_actions ),
							spirit_namespace::space_p );
		semantic_actions.check_unit_data();
	} catch (ExcInvalidUnit &e) {
		e << EI_UnitDefinition(s);
		throw;
	}

    return semantic_actions.unit_data();
}


double UnitConverter::convert(std::string actual_unit) {
	unit_si_.reset();
	coef_ = 1.0;
	UnitData unit_data = read_unit(actual_unit);

	Formula &formula = unit_data.find("")->second;
	for( std::vector<struct Factor>::iterator it = formula.factors_.begin(); it !=formula.factors_.end(); ++it ) {
		add_converted_unit(*it, unit_data, unit_si_, coef_);
	}

	return coef_;
}


void UnitConverter::add_converted_unit(Factor factor, UnitData &unit_data, UnitSI &unit_si, double &coef) {
	if (factor.basic_) {
		std::map<std::string, struct BasicFactors::DerivedUnit>::const_iterator it = UnitConverter::basic_factors.units_map_.find(factor.factor_);
		ASSERT_DBG(it != UnitConverter::basic_factors.units_map_.end())(factor.factor_).error("Undefined unit.");
		coef *= pow(it->second.coef_, factor.exponent_);
		unit_si.multiply(it->second.unit_, factor.exponent_);
	} else {
		std::map<std::string, struct Formula>::iterator it = unit_data.find(factor.factor_);
		ASSERT_DBG(it != unit_data.end())(factor.factor_).error("Undefined unit.");
		coef *= pow(it->second.coef_, factor.exponent_);
		for( std::vector<struct Factor>::iterator in_it = it->second.factors_.begin(); in_it !=it->second.factors_.end(); ++in_it ) {
			Factor new_factor = Factor(in_it->factor_, in_it->exponent_*factor.exponent_, in_it->basic_ );
			add_converted_unit(new_factor, unit_data, unit_si, coef);
		}
	}
}

