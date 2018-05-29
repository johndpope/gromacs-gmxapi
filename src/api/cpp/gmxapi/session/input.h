//
// Created by Eric Irrgang on 5/25/18.
//

#ifndef GMXAPI_SESSION_INPUT_H
#define GMXAPI_SESSION_INPUT_H

#include <memory>
#include <string>

#include "gmxapi/status.h"

namespace gmxapi {
namespace session {

/*!
 * \brief Extensible adapter to output data streams for gmxapi client code running in Sessions.
 *
 * Code that is launched as part of a gmxapi Session receives a bundle of resources for the Session. Input and output
 * "ports" registered by the client code appear as collections of publish and visit functions with two arguments:
 * one naming the port, and one providing the data to publish or the memory into which to receive data. The type of the
 * data argument must match the registered type of the named port.
 *
 * Objects of this type are not created by API client code, but are received during session launch.
 *
 * /internal
 * I don't really want to do run-time type checking, so I think we need to just decide that the data types of graph
 * edges are specified in the API. We can provide one or two more free-form data types to allow flexibilty, and users
 * can request officially-supported data types if it makes sense. I think that a robust matrix class like Eigen provides
 * should be sufficient for binary data and we can provide a string type for arbitrary serializeable data. Key-value
 * maps and simple scalars also make sense.
 *
 */
class Input final
{
    public:
        Input(const Input&) = delete;
        Input(Input&&) = default;
        Input& operator=(const Input&) = delete;
        Input& operator=(Input&&) = default;
        ~Input();

        // Opaque implementation class.
        class Impl;

        /*!
         * \brief Set data for a registered output stream.
         *
         * \param inputName Registered name of the output port
         * \param data data to set with the registered output handler.
         *
         * We should not use a template here to handle the different data types because the template might be expressed
         * with different munged symbol names by different compilers. But we want this interface to be extensible, so
         * we need to consider how to allow flexible types. We could wrap all data in a gmxapi::Data wrapper or something,
         * but that makes calling the set() method more cumbersome in the client code.
         *
         * What we could do, though, is to provide a template function as a helper that is compiled in the client code
         * and just makes it easier to make the correct call here. Then a gmxapi::Data wrapper wouldn't be cumbersome.
         */
        gmxapi::Status get(const std::string &inputName,
                           bool& data);
        gmxapi::Status get(const std::string &inputName,
                           double& data);

        /*!
         * \brief Wrapper to accept const C-string arguments
         *
         * \tparam T output stream data type.
         * \param inputName registered output port name.
         * \param data data to set with the registered output handler.
         */
        template<class T>
        void get(const char* inputName, T& data)
        {
            this->get(std::string(inputName), data);
        }

    private:
        // opaque pointer to implementation.
        std::unique_ptr<Impl> impl_;

        // Private constructor. Objects of this type are provided by the framework and are a detail of the Context
        // implementation.
        explicit Input(std::unique_ptr<Impl>&& implementation);
};
} // end namespace gmxapi::session
} // end namespace gmxapi

#endif //GMXAPI_SESSION_INPUT_H
