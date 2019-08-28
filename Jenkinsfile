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
		    cmakeBuild buildType: 'Release', buildDir: 'build', cleanBuild: true, installation: 'InSearchPath', steps: [[withCmake: true]]
		}
	    }
	}
	stage('Archive') {
	    steps {
		// Publish Doxygen Documentation.
		//publishHTML([allowMissing: false, alwaysLinkToLastBuild: true, keepAll: false, reportDir: 'paracuber/doc/html', reportFiles: '**', reportName: 'Doxygen Documentation', reportTitles: ''])
	    }
	}
    }
}
