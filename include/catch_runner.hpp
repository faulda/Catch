/*
 *  Created by Phil on 31/10/2010.
 *  Copyright 2010 Two Blue Cubes Ltd. All rights reserved.
 *
 *  Distributed under the Boost Software License, Version 1.0. (See accompanying
 *  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 */
#ifndef TWOBLUECUBES_CATCH_RUNNER_HPP_INCLUDED
#define TWOBLUECUBES_CATCH_RUNNER_HPP_INCLUDED

#include "internal/catch_commandline.hpp"
#include "internal/catch_list.hpp"
#include "internal/catch_runner_impl.hpp"
#include "internal/catch_test_spec.h"

#include <fstream>
#include <stdlib.h>
#include <limits>

namespace Catch {

    class Runner2 { // This will become Runner when Runner becomes Context

    public:
        Runner2( Config& configWrapper )
        :   m_configWrapper( configWrapper ),
            m_config( configWrapper.data() )
        {
            resolveStream();
            makeReporter();
        }

        Totals runTests() {

            std::vector<TestCaseFilters> filterGroups = m_config.filters;
            if( filterGroups.empty() ) {
                TestCaseFilters filterGroup( "" );
                filterGroup.addFilter( TestCaseFilter( "./*", IfFilterMatches::ExcludeTests ) );
                filterGroups.push_back( filterGroup );
            }

            Runner context( m_configWrapper, m_reporter ); // This Runner will be renamed Context
            Totals totals;

            std::vector<TestCaseFilters>::const_iterator it = filterGroups.begin();
            std::vector<TestCaseFilters>::const_iterator itEnd = filterGroups.end();
            for(; it != itEnd; ++it ) {
                m_reporter->StartGroup( it->getName() );
                runTestsForGroup( context, *it );
                if( context.aborting() )
                    m_reporter->Aborted();
                m_reporter->EndGroup( it->getName(), totals );
            }
            return totals;
        }

        Totals runTestsForGroup( Runner& context, const TestCaseFilters& filterGroup ) {
            Totals totals;
            std::vector<TestCaseInfo>::const_iterator it = getRegistryHub().getTestCaseRegistry().getAllTests().begin();
            std::vector<TestCaseInfo>::const_iterator itEnd = getRegistryHub().getTestCaseRegistry().getAllTests().end();
            int testsRunForGroup = 0;
            for(; it != itEnd; ++it ) {
                if( filterGroup.shouldInclude( *it ) ) {
                    testsRunForGroup++;
                    if( m_testsAlreadyRun.find( *it ) == m_testsAlreadyRun.end() ) {

                        if( context.aborting() )
                            break;

                        totals += context.runTest( *it );
                        m_testsAlreadyRun.insert( *it );
                    }
                }
            }
            if( testsRunForGroup == 0 )
                std::cerr << "\n[No test cases matched with: " << filterGroup.getName() << "]" << std::endl;
            return totals;
            
        }

    private:
        void resolveStream() {
            if( !m_config.stream.empty() ) {
                if( m_config.stream[0] == '%' )
                    m_configWrapper.useStream( m_config.stream.substr( 1 ) );
                else
                    m_configWrapper.setFilename( m_config.stream );
            }
            // Open output file, if specified
            if( !m_config.outputFilename.empty() ) {
                m_ofs.open( m_config.outputFilename.c_str() );
                if( m_ofs.fail() ) {
                    std::ostringstream oss;
                    oss << "Unable to open file: '" << m_config.outputFilename << "'";
                    throw std::domain_error( oss.str() );
                }
                m_configWrapper.setStreamBuf( m_ofs.rdbuf() );
            }
        }
        void makeReporter() {
            std::string reporterName = m_config.reporter.empty()
            ? "basic"
            : m_config.reporter;

            ReporterConfig reporterConfig( m_config.name, m_configWrapper.stream(), m_config.includeWhichResults == Include::SuccessfulResults );

            m_reporter = getRegistryHub().getReporterRegistry().create( reporterName, reporterConfig );
            if( !m_reporter ) {
                std::ostringstream oss;
                oss << "No reporter registered with name: '" << reporterName << "'";
                throw std::domain_error( oss.str() );
            }
        }
        
    private:
        Config& m_configWrapper;
        const ConfigData& m_config;
        std::ofstream m_ofs;
        Ptr<IReporter> m_reporter;
        std::set<TestCaseInfo> m_testsAlreadyRun;
    };

    inline int Main( Config& configWrapper ) {
        int result = 0;
        try
        {
            Runner2 runner( configWrapper );

            const ConfigData& config = configWrapper.data();

            // Handle list request
            if( config.listSpec != List::None ) {
                List( config );
                return 0;
            }

            result = static_cast<int>( runner.runTests().assertions.failed );

        }
        catch( std::exception& ex ) {
            std::cerr << ex.what() << std::endl;
            result = (std::numeric_limits<int>::max)();
        }

        Catch::cleanUp();
        return result;
    }

    inline void showUsage( std::ostream& os ) {
        os  << "\t-?, -h, --help\n"
            << "\t-l, --list [all | tests | reporters [xml]]\n"
            << "\t-t, --test <testspec> [<testspec>...]\n"
            << "\t-r, --reporter <reporter name>\n"
            << "\t-o, --out <file name>|<%stream name>\n"
            << "\t-s, --success\n"
            << "\t-b, --break\n"
            << "\t-n, --name <name>\n"
            << "\t-a, --abort [#]\n"
            << "\t-nt, --nothrow\n\n"
            << "For more detail usage please see: https://github.com/philsquared/Catch/wiki/Command-line" << std::endl;    
    }
    inline void showHelp( std::string exeName ) {
        std::string::size_type pos = exeName.find_last_of( "/\\" );
        if( pos != std::string::npos ) {
            exeName = exeName.substr( pos+1 );
        }

        std::cout << exeName << " is a CATCH host application. Options are as follows:\n\n";
        showUsage( std::cout );
    }
    
    inline int Main( int argc, char* const argv[], Config& config ) {

        try {
            CommandParser parser( argc, argv );

            AllOptions options;
            
            if( Command cmd = parser.find( "-h", "-?", "--help" ) ) {
                if( cmd.argsCount() != 0 )
                    cmd.raiseError( "Does not accept arguments" );

                showHelp( argv[0] );
                Catch::cleanUp();        
                return 0;
            }
        
            options.parseIntoConfig( parser, config.data() );
        }
        catch( std::exception& ex ) {
            std::cerr << ex.what() << "\n\nUsage: ...\n\n";
            showUsage( std::cerr );
            Catch::cleanUp();
            return (std::numeric_limits<int>::max)();
        }
                
        return Main( config );
    }
    
    inline int Main( int argc, char* const argv[] ) {
        Config config;
// !TBD: This doesn't always work, for some reason
//        if( isDebuggerActive() )
//            config.useStream( "debug" );
        return Main( argc, argv, config );
    }
    
} // end namespace Catch

#endif // TWOBLUECUBES_CATCH_RUNNER_HPP_INCLUDED
