pipeline {
    agent any
    options {
	buildDiscarder(logRotator(numToKeepStr: '10'))
    }

    dir('paracuber') {
	stages {
	    stage('Build') {
		steps {
		    cmake installation: 'InSearchPath'
		    cmakeBuild buildType: 'Release', cleanBuild: true, installation: 'InSearchPath', steps: [[withCmake: true]]
		}
	    }
	}	
    }
}
