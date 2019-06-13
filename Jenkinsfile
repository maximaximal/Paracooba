pipeline {
    agent any
    options {
	buildDiscarder(logRotator(numToKeepStr: '10'))
    }

    stages {
	stage('Build') {
	    steps {
		dir('paracuber') {
		    cmake installation: 'InSearchPath'
		    cmakeBuild buildType: 'Release', cleanBuild: true, installation: 'InSearchPath', steps: [[withCmake: true]]
		}
	    }
	}
    }	
}
