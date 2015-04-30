#ifndef __IRODS_FIRST_CLASS_OBJECT_HPP__
#define __IRODS_FIRST_CLASS_OBJECT_HPP__

// =-=-=-=-=-=-=-
#include "irods_log.hpp"
#include "irods_resource_types.hpp"
#include "irods_network_types.hpp"

// =-=-=-=-=-=-=-
// irods includes
#include "rcConnect.h"

// =-=-=-=-=-=-=-
// boost includs
#include <boost/shared_ptr.hpp>

namespace irods {
// =-=-=-=-=-=-=-
// base class for all object types
    class first_class_object {
        public:
            // =-=-=-=-=-=-=-
            // Constructors
            first_class_object() {};

            // =-=-=-=-=-=-=-
            // Destructor
            virtual ~first_class_object() {};

            // =-=-=-=-=-=-=-
            // plugin resolution operators
            virtual error resolve(
                const std::string&, // plugin interface
                plugin_ptr& ) = 0;  // resolved plugin

            // =-=-=-=-=-=-=-
            // accessor for rule engine variables
            virtual error get_re_vars( keyValPair_t& ) = 0;

    }; // class first_class_object

/// =-=-=-=-=-=-=-
/// @brief shared pointer to first_class_object
    typedef boost::shared_ptr< first_class_object > first_class_object_ptr;

}; // namespace irods

#endif // __IRODS_FIRST_CLASS_OBJECT_HPP__



