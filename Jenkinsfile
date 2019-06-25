pipeline {
    agent any
    options {
	buildDiscarder(logRotator(numToKeepStr: '10'))
    }

    stages {
	stage('Build') {
	    steps {
		dir('paracuber') {
		    sh 'mkdir -p build/third_party/cadical-out/build/ && touch build/third_party/cadical-out/build/libcadical.a'
		    cmakeBuild buildType: 'Release', cleanBuild: true, installation: 'InSearchPath', steps: [[withCmake: true]]
		}
	    }
	}
	stage('Archive') {
	    steps {
		// Publish Doxygen Documentation.
		publishHTML (target: {
		    allowMissing: false,
		    alwaysLinkToLastBuild: false,
		    keepAll: true,
		    reportDir: 'build/doc/html',
		    reportName: 'Doxygen Documentation'
		})
	    }
	}
    }	
}
