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
 * @file    unit_si.hh
 * @brief   
 */

#ifndef UNIT_SI_HH_
#define UNIT_SI_HH_

#include <vector>
#include <string>


/**
 * @brief Class for representation SI units of Fields.
 *
 * Units are set through exponents of basic SI units. These exponents are set in methods with same
 * name as unit symbols (e.g. kg(), K() etc).
 *
 * Class contains method that provides formated string representing full unit symbol (usable in
 * LaTeX output).
 *
 * UnitSI object contains flag that says if it is defined
 * - undefined object can't be formated
 * - if any exponent is set, flag is set to defined
 *
 * Class contains static methods that return frequently used derived units (Watt, Pascal etc).
 */
class UnitSI {
public:
	/// Constructor
	UnitSI();

	/// Methods return frequently used derived units
	/// Returns Newton
	static UnitSI & N();
	/// Returns Joule
	static UnitSI & J();
	/// Returns Watt
	static UnitSI & W();
	/// Returns Pascal
	static UnitSI & Pa();
	/// Returns dimensionless unit
	static UnitSI & dimensionless();
	/// Returns dimensionless unit
	static UnitSI & one();

	/// Methods set values of exponents for SI units with similar name
	UnitSI & m(int exp = 1);
	UnitSI & kg(int exp = 1);
	UnitSI & s(int exp = 1);
	UnitSI & A(int exp = 1);
	UnitSI & K(int exp = 1);
	UnitSI & mol(int exp = 1);
	UnitSI & cd(int exp = 1);
	/// The dimension dependent meter: md^y = m^(yd), where 'd' is dimension.
	UnitSI & md(int exp = -1);

	/**
	 * Makes unit description string in Latex format, e.g. "$[m.kg^{2}.s^{-2}]$"
	 *
	 * Have assert for undefined units.
	 */
	std::string format_latex() const;

	std::string format_text() const;

    /**
     * Machine readable JSON format. Units are stored as a record with keys given by
     * SI units strings (corresponding to setters). Values of the keys are integer.
     */
    std::string json() const;

	/**
	 * Set flag that unit is undefined.
	 *
	 * Default value is true (set in constructor).
	 * If any exponent is set, @p undef_ flag is unset.
	 * In all fields unit must be defined by user.
	 */
	void undef(bool val = true);

	/// Return true if the unit is defined.
	bool is_def() const;

	/// Multiply with power of given unit
	void multiply(const UnitSI &other, int exp = 1);

	/// Reset UnitSI object (set vector of exponents to zeros and set undef flag)
	void reset();

	/**
	 * @brief Convert and check user-defined unit.
	 *
	 * - converts unit SI define by user as string
	 * - checks if converted unit is equal with self
	 * - return multiplicated coeficient
	 */
	double convert_unit_from(std::string actual_unit) const;

	/// Comparison operator
	bool operator==(const UnitSI &other) const;

private:
	/// Values determine positions of exponents in exponents_ vector
	enum UnitOrder {
		order_m=0,
		order_md=1,
		order_kg=2,
		order_s=3,
		order_A=4,
		order_K=5,
		order_mol=6,
		order_cd=7,
		n_base_units=8
	};

	/// Variable parts of output format. Used in the @p format method.
	struct OutputFormat {
	    std::string exp_open, exp_close, delimiter;
	};

    /**
     * Symbols for individual units. Can not use static variable due to usage in static initialization.
     */
	static const std::string &unit_symbol(unsigned int idx);

	/// Generic output formating method.
	std::string format(OutputFormat form) const;


	/**
	 * Stores exponents of base SI units in the order given by the
	 * UnitOrder enum
	 *
	 * where md represents value of exponent depended on dimension (m^{-d})
	 */
	std::vector<int> exponents_;


	/**
	 * Flag if object is undefined.
	 *
	 * Value is set on true in constructor, when any exponent is changed, false value is set.
	 */
	bool undef_;

	/// Product of two units.
	friend UnitSI operator *(const UnitSI &a, const UnitSI &b);
	/// Proportion of two units.
	friend UnitSI operator /(const UnitSI &a, const UnitSI &b);
};


/// Product of two units.
UnitSI operator *(const UnitSI &a, const UnitSI &b);

/// Proportion of two units.
UnitSI operator /(const UnitSI &a, const UnitSI &b);


#endif /* UNIT_SI_HH_ */
