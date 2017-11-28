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
 * @file    reader_internal_csv.cc
 * @brief
 */


#include "input/reader_internal_csv.hh"
#include "input/input_type.hh"
#include "input/csv_tokenizer.hh"

namespace Input {

using namespace std;

/*******************************************************************
 * implementation of ReaderInternalCsvInclude
 */

ReaderInternalCsvInclude::ReaderInternalCsvInclude()
{}

StorageBase * ReaderInternalCsvInclude::read_storage(PathBase &p, const Type::Array *array)
{
	if ( p.is_record_type() ) { // sub-type must be record type
		// load path to CSV file
		std::string included_file;
        if ( p.down("file") ) {
       		included_file = get_included_file(p);
            p.up();
        } else {
        	this->generate_input_error(p, array, "Missing key 'file' defines including input file.", false);
        }

        // number of head lines to skip
        unsigned int n_head_lines = 0;
        if ( p.down("n_head_lines") ) {
        	try {
        		n_head_lines = p.get_int_value();
        	}
			catch (ExcInputError & e) {
				complete_input_error(e, p, ValueTypes::int_type);
				e << EI_InputType("number of lines to skip");
				throw;
			}
        	p.up();
        }

        // open CSV file, get number of lines, skip head lines
        FilePath fp((included_file), FilePath::input_file);
        CSVTokenizer tok( fp );

        const Type::TypeBase &sub_type = array->get_sub_type(); // sub-type of array
        StorageBase *item_storage; // storage of sub-type record of included array
        csv_columns_map_.clear();
        if ( p.down("format") ) {
			try {
				csv_subtree_depth_ = p.path_.size();
				item_storage = make_storage(p, &sub_type);
			} catch (ExcMultipleDefinitionCsvColumn &e) {
				e << EI_File(tok.f_name());
				throw;
			}
            p.up();
        } else {
        	this->generate_input_error(p, array, "Missing key 'format' defines mapping column of CSV file to input subtree.", false);
        }

        // get value of maximal column index in map
        map<unsigned int, IncludeCsvData>::iterator it;
        it = csv_columns_map_.end(); --it;
        unsigned int max_column_index = it->first;

        unsigned int n_lines = tok.get_n_lines() - n_head_lines;
        tok.skip_header(n_head_lines);
        StorageArray *storage_array = new StorageArray(n_lines);
        std::set<unsigned int> unused_columns;
        for( unsigned int arr_item=0; arr_item < n_lines; ++arr_item) {
        	unsigned int i_col;
        	tok.next_line();
        	for (i_col=0; !tok.eol(); ++i_col, ++tok) {
        		it = csv_columns_map_.find(i_col);
        		if (it != csv_columns_map_.end()) {
        			switch (it->second.data_type) {
						case IncludeDataTypes::type_int: {
							int val;
							try {
								val = tok.get_int_val();
							} catch (ExcWrongCsvFormat &e) {
								e << EI_Specification("Wrong integer value");
								e << EI_ErrorAddress(p.as_string());
								throw;
							}

							const Type::Integer *int_type = static_cast<const Type::Integer *>(it->second.type);
							if ( !int_type->match(val) ) {
								THROW( ExcWrongCsvFormat() << EI_Specification("Integer value out of bounds")
										<< EI_TokenizerMsg(tok.position_msg()) << EI_ErrorAddress(p.as_string()) );
							}
							set_storage_from_csv( i_col, item_storage, new StorageInt(val) );
							break;
						}
						case IncludeDataTypes::type_double: {
							double val;
							try {
								val = tok.get_double_val();
							} catch (ExcWrongCsvFormat &e) {
								e << EI_ErrorAddress(p.as_string());
								throw;
							}

							const Type::Double *double_type = static_cast<const Type::Double *>(it->second.type);
							if ( !double_type->match(val) ) {
								THROW( ExcWrongCsvFormat() << EI_Specification("Double value out of bounds")
										<< EI_TokenizerMsg(tok.position_msg()) << EI_ErrorAddress(p.as_string()) );
							}
							set_storage_from_csv( i_col, item_storage, new StorageDouble(val) );
							break;
						}
						case IncludeDataTypes::type_bool: {
							int val;
							try {
								val = tok.get_int_val();
							} catch (ExcWrongCsvFormat &e) {
								e << EI_Specification("Wrong boolean value");
								e << EI_ErrorAddress(p.as_string());
								throw;
							}
							set_storage_from_csv( i_col, item_storage, new StorageBool(val) );
							break;
						}
						case IncludeDataTypes::type_string: {
							try {
								set_storage_from_csv( i_col, item_storage, new StorageString(tok.get_string_val()) );
							} catch (ExcWrongCsvFormat &e) {
								e << EI_Specification("Wrong string value");
								e << EI_ErrorAddress(p.as_string());
								throw;
							}
							break;
						}
						case IncludeDataTypes::type_sel: {
							const Type::Selection *selection = static_cast<const Type::Selection *>(it->second.type);
							try {
								std::string item_name = tok.get_string_val();
								int val = selection->name_to_int( item_name );
								set_storage_from_csv( i_col, item_storage, new StorageInt(val) );
							} catch (ExcWrongCsvFormat &e) {
								e << EI_Specification("Wrong selection value");
								e << EI_ErrorAddress(p.as_string());
								throw;
							} catch (Type::Selection::ExcSelectionKeyNotFound &exc) {
								THROW( ExcWrongCsvFormat() << EI_Specification("Wrong selection value")
										<< EI_TokenizerMsg(tok.position_msg()) << EI_ErrorAddress(p.as_string()) );
							}
							break;
						}
        			}
        		} else {
        			// add index of unused column
        			unused_columns.insert(i_col);
        		}
        	}
    		if ( max_column_index > (i_col-1) ) {
    			this->generate_input_error(p, array, "Count of columns in CSV file is less than expected index, defined on input.", false);
    		}
            storage_array->new_item(arr_item, item_storage->deep_copy() );
        }

        if (unused_columns.size()) { // print warning with indexes of unused columns
        	stringstream ss;
        	for (std::set<unsigned int>::iterator it=unused_columns.begin(); it!=unused_columns.end(); ++it)
        		ss << (*it) << " ";
            WarningOut().fmt("Unused columns: {}\nin imported CSV input file: {}\n", ss.str(), tok.f_name());
        }
        return storage_array;

	} else {
		this->generate_input_error(p, array, "Invalid definition of CSV include.", false);
	}
	return NULL;
}

StorageBase * ReaderInternalCsvInclude::make_sub_storage(PathBase &p, const Type::Array *array)
{
	this->generate_input_error(p, array, "Array type in CSV-included part of IST is forbidden!\n", false);
    return NULL;
}

StorageBase * ReaderInternalCsvInclude::make_sub_storage(PathBase &p, const Type::Selection *selection)
{
	const Type::Integer *format_int = new Type::Integer(0);
	std::int64_t pos = read_int_value(p, format_int);

	IncludeCsvData include_data;
	include_data.data_type = IncludeDataTypes::type_sel;
	include_data.storage_indexes = create_indexes_vector(p);
	include_data.type = selection;
	if (csv_columns_map_.find(pos)!=csv_columns_map_.end()) {
		THROW( ExcMultipleDefinitionCsvColumn() << EI_ColumnIndex(pos) << EI_ErrorAddress(p.as_string()) );
	} else {
		csv_columns_map_[pos] = include_data;
	}
	delete format_int;
	return new StorageInt( 0 );
}

StorageBase * ReaderInternalCsvInclude::make_sub_storage(PathBase &p, const Type::Bool *bool_type)
{
	const Type::Integer *format_int = new Type::Integer(0);
	std::int64_t pos = read_int_value(p, format_int);

	IncludeCsvData include_data;
	include_data.data_type = IncludeDataTypes::type_bool;
	include_data.storage_indexes = create_indexes_vector(p);
	include_data.type = bool_type;
	if (csv_columns_map_.find(pos)!=csv_columns_map_.end()) {
		THROW( ExcMultipleDefinitionCsvColumn() << EI_ColumnIndex(pos) << EI_ErrorAddress(p.as_string()) );
	} else {
		csv_columns_map_[pos] = include_data;
	}
	delete format_int;

	return new StorageBool( false );
}

StorageBase * ReaderInternalCsvInclude::make_sub_storage(PathBase &p, const Type::Integer *int_type)
{
	const Type::Integer *format_int = new Type::Integer(0);
	std::int64_t value = read_int_value(p, format_int);

	IncludeCsvData include_data;
	include_data.data_type = IncludeDataTypes::type_int;
	include_data.storage_indexes = create_indexes_vector(p);
	include_data.type = int_type;
	if (csv_columns_map_.find(value)!=csv_columns_map_.end()) {
		THROW( ExcMultipleDefinitionCsvColumn() << EI_ColumnIndex(value) << EI_ErrorAddress(p.as_string()) );
	} else {
		csv_columns_map_[value] = include_data;
	}
	delete format_int;

	return new StorageInt( 0 );
}

StorageBase * ReaderInternalCsvInclude::make_sub_storage(PathBase &p, const Type::Double *double_type)
{
	const Type::Integer *format_int = new Type::Integer(0);
	std::int64_t pos = read_int_value(p, format_int);

	IncludeCsvData include_data;
	include_data.data_type = IncludeDataTypes::type_double;
	include_data.storage_indexes = create_indexes_vector(p);
	include_data.type = double_type;
	if (csv_columns_map_.find(pos)!=csv_columns_map_.end()) {
		THROW( ExcMultipleDefinitionCsvColumn() << EI_ColumnIndex(pos) << EI_ErrorAddress(p.as_string()) );
	} else {
		csv_columns_map_[pos] = include_data;
	}
	delete format_int;

	return new StorageDouble( 0.0 );
}

StorageBase * ReaderInternalCsvInclude::make_sub_storage(PathBase &p, const Type::String *string_type)
{
	try {
		const Type::Integer *format_int = new Type::Integer(0);
		std::int64_t pos = read_int_value(p, format_int);

		IncludeCsvData include_data;
		include_data.data_type = IncludeDataTypes::type_string;
		include_data.storage_indexes = create_indexes_vector(p);
		include_data.type = string_type;
		if (csv_columns_map_.find(pos)!=csv_columns_map_.end()) {
			THROW( ExcMultipleDefinitionCsvColumn() << EI_ColumnIndex(pos) << EI_ErrorAddress(p.as_string()) );
		} else {
			csv_columns_map_[pos] = include_data;
		}
		delete format_int;

		return new StorageString("");
	} catch (ExcInputError & e) {
		// no error, string value is not forbidden in CSV include
		return new StorageString( read_string_value(p, string_type) );
	}
}

vector<unsigned int> ReaderInternalCsvInclude::create_indexes_vector(PathBase &p)
{
	vector<unsigned int> csv_storage_indexes( p.path_.size()-csv_subtree_depth_ );
	for (unsigned int i_source=csv_subtree_depth_, i_target=0; i_source<p.path_.size(); ++i_source, ++i_target ) {
		ASSERT_GE(p.path_[i_source].first, 0).error();
		csv_storage_indexes[i_target] = p.path_[i_source].first;
	}
	return csv_storage_indexes;
}

void ReaderInternalCsvInclude::set_storage_from_csv(unsigned int column_index, StorageBase * item_storage, StorageBase * new_storage)
{
	map<unsigned int, IncludeCsvData>::iterator it = csv_columns_map_.find(column_index);
	ASSERT(it!=csv_columns_map_.end()).error();

	unsigned int i;
	StorageBase *loop_storage = item_storage;
	for (i=0; i<it->second.storage_indexes.size()-1; ++i) loop_storage = loop_storage->get_item( it->second.storage_indexes[i] );
	loop_storage->set_item( it->second.storage_indexes[i], new_storage );
}


} // namespace Input